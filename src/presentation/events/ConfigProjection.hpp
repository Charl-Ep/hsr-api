#pragma once
#include <cstddef>
#include <map>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace hksr::sdk_v1::detail {
// Copy-on-write planning over an explicitly selected CONFIG graph, never an
// entity/world object graph. The managed adapter owns all temporary roots and
// defines which fields/array slots are configuration edges. No reflection,
// policy-by-name, task execution or publication is hidden in this algorithm.
struct ConfigEdge { std::size_t slot; void* target; };
struct ConfigView { bool disable_task = false; std::vector<ConfigEdge> edges; };
struct ConfigProjectionAccess {
    virtual ~ConfigProjectionAccess() = default;
    virtual ConfigView Inspect(void* source) = 0;
    // Shallow copy of the exact runtime type, retained by the adapter. Source
    // must remain untouched even if a later allocation/write fails.
    virtual void* Copy(void* source) = 0;
    virtual void SetEdge(void* copy, std::size_t slot, void* target) = 0;
    virtual void DisableTask(void* copy) = 0;
};
struct ConfigProjection {
    std::vector<void*> roots;
    std::size_t inspected = 0, copied = 0, disabled = 0;
    struct Uninspected {void* source;std::string error;};
    std::vector<Uninspected> uninspected;
};
inline ConfigProjection ProjectConfig(const std::vector<void*>& roots, ConfigProjectionAccess& access) {
    ConfigProjection out;
    struct Node {
        ConfigView view;
        std::vector<void*> parents;
        void* copy = nullptr;
        bool dirty = false;
    };
    std::map<void*, Node> nodes;
    std::vector<void*> pending;
    auto discover = [&](void* object) {
        if (object && nodes.emplace(object, Node{}).second) pending.push_back(object);
    };
    for (auto* root : roots) discover(root);
    // Index-based work queues handle shared children and cycles without
    // recursion or arbitrary depth limits. Each source is inspected once.
    for (std::size_t i = 0; i < pending.size(); ++i) {
        auto* object = pending[i];
        auto& node = nodes.at(object);
        try {node.view = access.Inspect(object);}
        catch(const std::bad_alloc&){throw;}
        catch(const std::exception& e) {
            // This branch remains the original object. A failed inspection
            // must not cancel independently established edits in its siblings.
            // It is reported as uninspected, never certified as policy-safe.
            out.uninspected.push_back({object,e.what()});continue;
        }
        for (const auto& edge : node.view.edges) {
            if (!edge.target) continue;
            discover(edge.target);
            nodes.at(edge.target).parents.push_back(object);
        }
    }
    std::vector<void*> changed;
    for (auto& item : nodes) if (item.second.view.disable_task) {
        item.second.dirty = true; changed.push_back(item.first);
    }
    // A parent may be encountered before its shared child is known to change.
    // Propagate through reverse edges before copying, including entire cycles.
    for (std::size_t i = 0; i < changed.size(); ++i)
        for (auto* parent : nodes.at(changed[i]).parents) {
            auto& node = nodes.at(parent);
            if (!node.dirty) { node.dirty = true; changed.push_back(parent); }
        }
    std::map<void*, void*> copies;
    for (auto* source : changed) {
        auto* copy = access.Copy(source);
        if (!copy || nodes.count(copy) || !copies.emplace(copy, source).second)
            throw std::runtime_error("configuration copier returned a source or reused copy identity");
        nodes.at(source).copy = copy;
    }
    out.inspected = nodes.size()-out.uninspected.size(); out.copied = changed.size();
    // All destinations exist before rewiring, so aliases/cycles retain their
    // identity relations. Unchanged subgraphs keep their original references.
    for (auto* source : changed) {
        const auto& node = nodes.at(source);
        for (const auto& edge : node.view.edges) {
            if (!edge.target) continue;
            const auto& child = nodes.at(edge.target);
            if (child.dirty) access.SetEdge(node.copy, edge.slot, child.copy);
        }
        if (node.view.disable_task) { access.DisableTask(node.copy); ++out.disabled; }
    }
    for (auto* root : roots) {
        const auto found = nodes.find(root);
        out.roots.push_back(found != nodes.end() && found->second.dirty ? found->second.copy : root);
    }
    return out; // Caller publishes only after this complete result exists.
}
}
