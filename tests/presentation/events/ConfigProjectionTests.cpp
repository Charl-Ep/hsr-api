#include "../../../src/presentation/events/ConfigProjection.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
using namespace hksr::sdk_v1::detail;
void Check(bool value) { if (!value) throw std::runtime_error("config projection test failed"); }
struct Config {
    bool enabled = true, block = false, unreadable = false;
    int payload = 47;
    std::vector<Config*> children;
};
struct Access : ConfigProjectionAccess {
    std::vector<std::unique_ptr<Config>> allocated;
    std::size_t reads = 0, writes = 0;
    bool bad_copy = false, fail_write = false;
    ConfigView Inspect(void* source) override {
        ++reads;auto& c = *static_cast<Config*>(source);
        if(c.unreadable)throw std::runtime_error("fixture branch unreadable");
        ConfigView out;out.disable_task = c.enabled && c.block;
        for (std::size_t i = 0; i < c.children.size(); ++i) out.edges.push_back({i, c.children[i]});
        return out;
    }
    void* Copy(void* source) override {
        if (bad_copy) return source;
        allocated.push_back(std::make_unique<Config>(*static_cast<Config*>(source)));
        return allocated.back().get();
    }
    void SetEdge(void* copy, std::size_t slot, void* target) override {
        if (fail_write) throw std::runtime_error("private write failed");
        ++writes;static_cast<Config*>(copy)->children.at(slot) = static_cast<Config*>(target);
    }
    void DisableTask(void* copy) override { ++writes;static_cast<Config*>(copy)->enabled = false; }
};
}
void TestConfigProjection() {
    // Event array -> two events -> shared nested task list -> disabled leaf.
    // A second root references the same leaf; a kept branch is not copied.
    Config leaf;leaf.block = true;
    Config kept, branch;branch.children = {&leaf, nullptr, &kept};
    Config a,b;a.children = {&branch};b.children = {&branch};
    Config root;root.children = {&a,&b,&kept};
    Access access;
    const auto result = ProjectConfig({&root,&leaf,nullptr},access);
    Check(result.inspected == 6 && result.copied == 5 && result.disabled == 1 && access.reads == 6);
    auto* r = static_cast<Config*>(result.roots[0]);
    auto* projected = static_cast<Config*>(result.roots[1]);
    Check(r != &root && projected != &leaf && !projected->enabled && projected->payload == leaf.payload);
    Check(r->children[0]->children[0] == r->children[1]->children[0]);
    Check(r->children[0]->children[0]->children[0] == projected);
    Check(r->children[0]->children[0]->children[1] == nullptr && r->children[2] == &kept && result.roots[2] == nullptr);
    Check(leaf.enabled && branch.children[0] == &leaf && a.children[0] == &branch && root.children[0] == &a);
    // No changes preserves the root and every identity, not a gratuitous deep copy.
    Access unchanged;Config clean;clean.children = {&kept};
    const auto same = ProjectConfig({&clean},unchanged);
    Check(same.roots[0] == &clean && same.copied == 0 && unchanged.writes == 0);
    // An existing disabled task is not toggled or copied again.
    Config disabled;disabled.enabled=false;disabled.block=true;Access already;
    Check(ProjectConfig({&disabled},already).copied == 0);
    // Cross-linked parents must not keep an edge back into the shared source.
    Config x,y;x.children={&y};y.children={&x,&leaf};Access cyclic;
    const auto cycle=ProjectConfig({&x,&y},cyclic);
    auto* xc=static_cast<Config*>(cycle.roots[0]);auto* yc=static_cast<Config*>(cycle.roots[1]);
    Check(cycle.copied==3 && xc->children[0]==yc && yc->children[0]==xc && x.children[0]==&y && y.children[0]==&x);
    // A bad adapter must never make DisableTask write the source object.
    Access bad;bad.bad_copy=true;bool failed=false;
    try { ProjectConfig({&leaf},bad); } catch(const std::exception&) { failed=true; }
    Check(failed && bad.writes==0 && leaf.enabled);
    // Incomplete private graphs are not returned for publication; source is intact.
    Access partial;partial.fail_write=true;failed=false;
    try { ProjectConfig({&root},partial); } catch(const std::exception&) { failed=true; }
    Check(failed && leaf.enabled && root.children[0]==&a && branch.children[0]==&leaf);
    Config unreadable;unreadable.unreadable=true;unreadable.children={&kept};
    Config mixed;mixed.children={&unreadable,&leaf};Access independent;
    const auto isolated=ProjectConfig({&mixed},independent);
    auto* mixed_copy=static_cast<Config*>(isolated.roots[0]);
    Check(isolated.uninspected.size()==1 && isolated.uninspected[0].source==&unreadable);
    Check(isolated.inspected==2 && isolated.copied==2 && isolated.disabled==1);
    Check(mixed_copy!=&mixed && mixed_copy->children[0]==&unreadable && !mixed_copy->children[1]->enabled);
    Check(mixed.children[1]==&leaf && leaf.enabled);
    Access empty;Check(ProjectConfig({nullptr},empty).inspected==0 && empty.reads==0);
    std::cout << "Config projection passed: selective copies, original immutability, shared references, cycles, nulls and failed publication. Synthetic data only.\n";
}
