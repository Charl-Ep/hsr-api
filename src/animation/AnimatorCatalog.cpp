#include <hksr/animation/AnimatorCatalog.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include "../../adapters/animation/AnimatorCatalogBindings.hpp"
#include <array>
#include <algorithm>
#include <stdexcept>

namespace hksr::api {
namespace {
using namespace detail;
void Require(bool ok, const char* error) {
    if (!ok)
        throw std::runtime_error(error);
}

}
struct AnimatorCatalog::State {
    AnimatorCatalogBindings bindings;
    ManagedRoots roots;
    ComponentConfig text;
    std::vector<ManagedRootId> pending;
    explicit State(RuntimeMetadata& metadata)
        : bindings(ResolveAnimatorCatalog(metadata)), roots(ResolveReferenceMethods(metadata)),
          text(ResolveComponentConfigMethods(metadata)) {}
    std::string Name(Method method, void* animator, int id, bool* roundtrip = nullptr) {
        void* value = nullptr;
        Require(native::Call(method, &value, animator, id).returned, "official name query failed");
        if (!value)
            return {};
        const auto root = roots.Keep(value);
        pending.push_back(root);
        Require(roots.Status(root).ready, "name root failed");
        auto result = text.ReadText(value);
        Require(result.read.returned, "name string read failed");
        if (roundtrip) {
            int actual = 0;
            *roundtrip = native::Call(bindings.hash, &actual, value).returned && actual == id;
        }
        if (roots.Release(root).complete) {
            roots.Forget(root);
            pending.pop_back();
        }
        return result.utf8;
    }
};
AnimatorCatalog::AnimatorCatalog(RuntimeMetadata& metadata) : state_(new State(metadata)) {}
AnimatorCatalog::~AnimatorCatalog() {
    delete state_;
}
std::size_t AnimatorCatalog::UnresolvedReferences() const {
    return state_->pending.size();
}
ControllerCatalog AnimatorCatalog::Read(void* animator) {
    auto& s = *state_;
    ControllerCatalog result;
    try {
        bool initialized = false;
        Require(animator && native::Call(s.bindings.initialized, &initialized, animator).returned && initialized,
                "Animator is not initialized");
        int layers = 0;
        Require(native::Call(s.bindings.layers, &layers, animator).returned, "official layer count unavailable");
        void* native = nullptr;
        Require(native::Call(s.bindings.cached, &native, animator).returned && native, "native Animator unavailable");
        result = ReadAnimatorController(s.bindings, native, layers);
        for (auto& layer : result.layers)
            try {
                layer.name = s.Name(s.bindings.layer_name, animator, layer.index);
            } catch (const std::exception& e) {
                result.issues.push_back("layer name: " + std::string(e.what()));
            }
        for (auto& row : result.states) {
            try {
                row.name = s.Name(s.bindings.resolve, animator, row.short_hash);
                row.path = s.Name(s.bindings.resolve, animator, row.path_hash);
                row.full_path = s.Name(s.bindings.resolve, animator, row.full_path_hash, &row.name_roundtrip);
                const auto accepted =
                    native::Call(s.bindings.has, &row.recognized, animator, row.layer, row.full_path_hash);
                if (!accepted.returned || !row.recognized || !row.name_roundtrip || row.full_path.empty())
                    result.issues.push_back("state identity not confirmed: layer=" + std::to_string(row.layer) +
                                            " index=" + std::to_string(row.index));
            } catch (const std::exception& e) {
                result.issues.push_back("state " + std::to_string(row.layer) + ":" + std::to_string(row.index) + " " +
                                        e.what());
            }
        }
    } catch (const std::exception& e) {
        result.issues.push_back(e.what());
    }
    for (auto it = s.pending.begin(); it != s.pending.end();) {
        if (s.roots.Release(*it).complete) {
            s.roots.Forget(*it);
            it = s.pending.erase(it);
        } else
            ++it;
    }
    result.complete = result.issues.empty();
    return result;
}
}
