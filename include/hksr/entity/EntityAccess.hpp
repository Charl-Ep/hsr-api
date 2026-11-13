#pragma once
#include <memory>
#include "EntityLifecycle.hpp"
#include "../managed/References.hpp"
#include <string>
#include <vector>
#include <utility>

namespace hksr::api {
struct EntityAccessMethods {
    Method runtime_id, owner_world, loaded, component;
    Method manager_lookup, kill_immediately, make_die;
    std::vector<std::string> binding_errors;
    Method entity_type, alive_state;
};
struct EntitySnapshot {
    Observed<std::uint32_t> runtime_id;
    Observed<GameWorld*> owner_world;
    Observed<bool> loaded;
};
struct RetirementResult {
    CallResult set_immediate, request;
    bool request_attempted = false;
};
// No implicit retries and no direct Dispose. Caller owns this entity, runs on
// its engine thread, and must not use stale pointers after a disposal event.
class EntityAccess {
  public:
    HKSR_API explicit EntityAccess(EntityAccessMethods methods);
    HKSR_API ~EntityAccess();
    HKSR_API EntityAccess(const EntityAccess&) noexcept;
    HKSR_API EntityAccess& operator=(const EntityAccess&) noexcept;
    HKSR_API EntitySnapshot Read(GameEntity* entity) const;
    HKSR_API Observed<int> Type(GameEntity* entity) const;
    HKSR_API Observed<int> AliveState(GameEntity* entity) const;
    HKSR_API Observed<GameEntity*> Lookup(EntityManager* manager, std::uint32_t id) const;
    HKSR_API ObjectResult Component(GameEntity* entity, void* managed_type) const;
    HKSR_API RetirementResult RequestRetirement(GameWorld* world, GameEntity* entity) const;

  private:
    std::shared_ptr<const EntityAccessMethods> methods_;
};
HKSR_API EntityAccessMethods ResolveEntityAccessMethods(MethodResolver& resolver);
}
