#pragma once
#include <hksr/entity/EntityOwner.hpp>
#include <hksr/entity/EntityAccess.hpp>
#include <hksr/managed/ManagedArrays.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <functional>
#include <memory>
#include <stdexcept>

namespace hksr::api::detail {
struct FaceMaterialComponentServices {
    std::function<bool()> current;
    std::function<ObjectResult()> find, create;
    std::function<void(void*)> initialize_created;
};
inline bool EnsureFaceMaterialComponent(const FaceMaterialComponentServices& services) {
    if (!services.current())
        throw std::runtime_error("face material owner ended");
    const auto existing = services.find();
    if (!existing.call.returned)
        throw std::runtime_error("face material component lookup failed");
    if (existing.object)
        return false;
    if (!services.current())
        throw std::runtime_error("face material owner changed during lookup");
    const auto created = services.create();
    if (!created.call.returned || !created.object)
        throw std::runtime_error("face material component creation failed");
    if (!services.current())
        throw std::runtime_error("face material owner ended during creation");
    services.initialize_created(created.object);
    return true;
}
// The engine owns the created component, its LateUpdate and model callbacks.
// This factory retains only the reusable reflection inputs, never an actor.
class FaceMaterialComponents {
  public:
    FaceMaterialComponents(RuntimeMetadata&, ReferenceMethods, EntityAccessMethods, ManagedArrays, EntityOwner);
    ~FaceMaterialComponents();
    void Ensure(EntityToken, GameWorld*);
    bool Close();

  private:
    struct State;
    std::unique_ptr<State> state_;
};
}
