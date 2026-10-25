#pragma once
#include <memory>
#include "EntityLifecycle.hpp"
#include "../metadata/RuntimeMetadata.hpp"
#include "../model/ModelRelease.hpp"
#include <utility>

namespace hksr::sdk_v1 {
struct EntityAccessMethods {
    Method runtime_id, owner_world, loaded, component;
    Method manager_lookup, kill_immediately, make_die;
    std::vector<std::string> binding_errors;
    Method entity_type,alive_state;
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
struct ModelAccessMethods {
    Method art, animator, load_state, changing, root_loaded, art_loaded;
    std::size_t root_offset = 0;
    bool has_root_field = false;
    std::vector<std::string> binding_errors;
};
struct ModelSnapshot {
    Observed<void*> root, art, animator;
    Observed<int> load_state; // raw enum, not a framework readiness policy
    Observed<bool> changing, root_loaded, art_loaded;
};
class ModelAccess {
public:
    HKSR_API explicit ModelAccess(ModelAccessMethods methods);
    HKSR_API ~ModelAccess();
    HKSR_API ModelAccess(const ModelAccess&) noexcept;
    HKSR_API ModelAccess& operator=(const ModelAccess&) noexcept;
    HKSR_API ModelSnapshot Read(CharacterModelComponent* model) const;
    HKSR_API Observed<void*> Art(CharacterModelComponent* model) const;
private:
    std::shared_ptr<const ModelAccessMethods> methods_;
};
HKSR_API EntityAccessMethods ResolveEntityAccessMethods(MethodResolver& resolver);
HKSR_API ModelAccessMethods ResolveModelAccessMethods(MethodResolver& resolver,
    const std::vector<DeclaredField>& fields);
// Locked-version contract retained from the previous model consumer. This is
// not the meaning of IsModelLoad/IsArtLoaded, nor automatic version detection.
HKSR_API Observed<bool> ModelLoadCompleted(const ModelSnapshot& snapshot);
}
