#pragma once
#include "ConfigProjection.hpp"
#include <hksr/managed/ManagedArrays.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/presentation/events/AnimationEventPolicy.hpp>
#include <functional>
#include <memory>

namespace hksr::api::detail {
// A schema selects reference fields, not arbitrary properties or object graphs.
// The task predicate is independent of copying. No managed method is executed
// by schema discovery; the access implementation uses reflection for writes.
struct ConfigObjectShape {
    std::vector<DeclaredField> edges;
    bool disable_task = false;
    DeclaredField enabled;
    struct StringField {
        DeclaredField field;
        AnimationStringKind kind;
    };
    std::vector<StringField> strings;
};
using ConfigShapeReader = std::function<ConfigObjectShape(void*, const DeclaredType&)>;
using ConfigStringTransform = std::function<AnimationStringEdit(AnimationStringKind, const std::string&)>;

// One construction transaction on the engine thread. Holds source, reflection
// descriptors and private copies until Close, including failed projections.
// The caller must keep this object until Close succeeds; destruction invokes
// no engine code. Result pointers may only outlive Close when the official
// consumer has adopted them through its own managed references.
class ManagedConfigProjection final : public ConfigProjectionAccess {
  public:
    ManagedConfigProjection(MethodResolver&, ObjectMetadata&, ManagedObjects&, ReferenceMethods, ArrayMethods,
                            ConfigShapeReader, ConfigStringTransform = {});
    ~ManagedConfigProjection();
    ManagedConfigProjection(const ManagedConfigProjection&) = delete;
    ManagedConfigProjection& operator=(const ManagedConfigProjection&) = delete;
    ConfigProjection Apply(const std::vector<void*>& roots);
    bool Close();
    ConfigView Inspect(void*) override;
    void* Copy(void*) override;
    void SetEdge(void*, std::size_t, void*) override;
    void DisableTask(void*) override;
    void Edit(void*) override;

  private:
    struct State;
    std::unique_ptr<State> state_;
};
}
