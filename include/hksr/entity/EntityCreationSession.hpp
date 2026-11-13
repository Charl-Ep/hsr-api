#pragma once
#include "EntityCreationTypes.hpp"
#include "EntityOwner.hpp"
#include "../managed/ManagedRoots.hpp"
#include "../world/WorldFrames.hpp"
#include "../managed/ManagedObjects.hpp"
#include <functional>

namespace hksr::api {
class RuntimeMetadata;
// Story creation requests official asynchronous model loading. A returned,
// owned entity is not a loaded model; continue Service and observe readiness.
class HKSR_API EntityCreationSession {
  public:
    EntityCreationSession(const EntityOwner&, Method map_factory, Method story_factory, ManagedObjects*,
                          EntityAccessMethods, ReferenceMethods, EntityCreationScope::BirthObserver = {},
                          Method partner_factory = {});
    EntityCreationSession(const EntityOwner& owner, Method map_factory, EntityAccessMethods access,
                          ReferenceMethods refs, EntityCreationScope::BirthObserver observer = {})
        : EntityCreationSession(owner, map_factory, {}, nullptr, std::move(access), refs, std::move(observer)) {}
    ~EntityCreationSession();
    EntityCreationSession(const EntityCreationSession&) = delete;
    EntityCreationSession& operator=(const EntityCreationSession&) = delete;
    void Create(const NpcCreateInput&);
    void Create(const StoryCreateInput&);
    // Creates an official Partner owned by this client, not a gameplay summon
    // task. Member naming, attachment and visual initialization are separate.
    void Create(const PartnerCreateInput&);
    void Remove();
    void Service(const WorldFrame&);
    const EntityCreationStatus& Status() const;
    bool IsStory() const;

  private:
    friend class PresentationContext;
    void PrepareStoryRuntime(RuntimeMetadata&, std::function<void(EntityToken, GameWorld*)>);
    void PreparePartnerRuntime(RuntimeMetadata&);
    void CreateOwned(GameWorld*, EntityManager*, bool require_birth, const std::function<CreateEntityResult()>&);
    struct State;
    State* state_;
};
HKSR_API Method ResolveStoryActorFactory(MethodResolver&);
HKSR_API Method ResolvePartnerFactory(MethodResolver&);
}
