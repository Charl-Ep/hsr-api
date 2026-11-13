#include "../../../src/presentation/audio/ActorAudioSource.hpp"
#include "../../../adapters/presentation/AudioPostBindings.hpp"
#include <map>
#include <iostream>
#include <stdexcept>

namespace {
using namespace hksr::api;
using hksr::api::detail::ActorAudioSource;
int info, next_root = 0, bindings = 0;
std::map<int, void*> held;
void Check(bool value) {
    if (!value)
        throw std::runtime_error("actor audio source assertion failed");
}
int __fastcall Keep(void* object, int, int, void*) {
    held[++next_root] = object;
    return next_root;
}
void* __fastcall Target(int id, void*) {
    return held.at(id);
}
void __fastcall Free(int id, void*) {
    Check(held.erase(id) == 1);
}
template <class F> Method M(F function) {
    return {reinterpret_cast<void*>(function), &info};
}
struct Component {
    void* art = nullptr;
};
struct Metadata final : ObjectMetadata {
    DeclaredType ReadObjectType(void*) override {
        throw std::runtime_error("unexpected type inventory");
    }
    std::vector<DeclaredField> ReadObjectFields(void*) override {
        ++bindings;
        return {{"", "EventComponent", "AAPEMMDGOJO", "UnityEngine.GameObject", offsetof(Component, art), 0, true}};
    }
};
void Test() {
    ManagedRoots roots({M(Keep), M(Target), M(Free), {}, {}});
    std::vector<ManagedRootId> retired;
    int old_art, new_art, latest_art;
    Component old_component{&old_art}, new_component{&new_art};
    Component* current = &old_component;
    void* indexed = nullptr;
    ActorAudioSource source;
    Metadata metadata;
    const auto bind = [&](void* object) -> ActorAudioSource::Reader {
        return hksr::api::detail::BindAnimationAudioEmitter(metadata, object);
    };
    const auto refresh = [&] {
        source.Refresh(current, roots, retired, bind, [&] { indexed = nullptr; });
        indexed = source.Emitter();
    };
    refresh();
    Check(indexed == &old_art && bindings == 1 && held.size() == 1);
    refresh();
    Check(bindings == 1); // current identity is checked without rebinding metadata
    current = &new_component;
    refresh();
    Check(indexed == &new_art && source.Emitter() != &old_art && bindings == 2);
    Check(old_component.art == &old_art && held.size() == 2 && retired.size() == 1);
    // The old component is still strongly rooted and still advertises old Art;
    // only the current entity-component identity selects the reader.
    for (const auto key : retired) {
        Check(roots.Release(key).complete);
        roots.Forget(key);
    }
    retired.clear();
    new_component.art = &latest_art;
    refresh();
    Check(indexed == &latest_art && bindings == 2);
    current = nullptr;
    refresh();
    Check(!indexed && !source.Emitter() && retired.size() == 1);
    current = &old_component;
    bool failed = false;
    try {
        source.Refresh(
            current, roots, retired,
            [](void*) -> ActorAudioSource::Reader { throw std::runtime_error("binding unavailable"); },
            [&] { indexed = nullptr; });
    } catch (const std::runtime_error&) {
        failed = true;
    }
    Check(failed && !source.Emitter() && !indexed); // no stale fallback after a failed replacement
    source.Reset(retired, [&] { indexed = nullptr; });
    for (const auto key : retired) {
        Check(roots.Release(key).complete);
        roots.Forget(key);
    }
    retired.clear();
    Check(held.empty());
}
}
int main() {
    try {
        Test();
        std::cout << "Actor audio source identity passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
