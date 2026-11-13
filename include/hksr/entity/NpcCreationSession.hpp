#pragma once
#include <hksr/entity/EntityAccess.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/world/WorldFrames.hpp>
#include "EntityOwner.hpp"
#include "EntityCreationTypes.hpp"

namespace hksr::api {
// One creation epoch, not a UI slot. Rebuild creates a new epoch after Close;
// saved placement/action intent belongs to the scene-instance manager.
// All methods are called on the owning engine thread. The caller keeps this
// object alive through disposal/cleanup; destruction NEVER calls the game.
// Requires the SDK lifecycle monitor for eventual Dispose completion.
class HKSR_API NpcCreationSession {
  public:
    NpcCreationSession(const EntityOwner& owner, Method factory, EntityAccessMethods access,
                       ReferenceMethods references, EntityCreationScope::BirthObserver observe_birth = {});
    ~NpcCreationSession();
    NpcCreationSession(const NpcCreationSession&) = delete;
    NpcCreationSession& operator=(const NpcCreationSession&) = delete;
    void Create(const NpcCreateInput& input);
    void Remove(); // intent only; submitted at the matching world frame
    void Service(const WorldFrame& frame);
    const EntityCreationStatus& Status() const;

  private:
    struct State;
    State* state_;
};
}
