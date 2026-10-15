#pragma once
#include <hksr/entity/EntityAccess.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/world/WorldFrames.hpp>
#include "EntityOwner.hpp"

namespace hksr::sdk_v1 {
struct NpcCreateInput {
    GameWorld* world = nullptr;
    EntityManager* manager = nullptr;
    MapNpcDefinition* definition = nullptr;
    NpcDataRow* row = nullptr;
    bool factory_option = true;
};
struct NpcEntityReceipt {
    EntityToken token;
    EntitySnapshot observation;
    RetirementResult retirement;
    bool retirement_started = false, disposed = false;
};
struct NpcCreationStatus {
    CreateEntityResult factory;
    CreationReceipt creation;
    std::vector<NpcEntityReceipt> entities;
    std::vector<std::string> errors;
    bool create_started = false, create_returned = false;
    bool removal_requested = false, closed = false;
};
// One creation epoch, not a UI slot. Rebuild creates a new epoch after Close;
// saved placement/action intent belongs to the scene-instance manager.
// All methods are called on the owning engine thread. The caller keeps this
// object alive through disposal/cleanup; destruction NEVER calls the game.
// Requires the SDK lifecycle monitor for eventual Dispose completion.
class HKSR_API NpcCreationSession {
public:
    NpcCreationSession(const EntityOwner& owner, Method factory, EntityAccessMethods access, ReferenceMethods references,
        EntityCreationScope::BirthObserver observe_birth={});
    ~NpcCreationSession();
    NpcCreationSession(const NpcCreationSession&) = delete;
    NpcCreationSession& operator=(const NpcCreationSession&) = delete;
    void Create(const NpcCreateInput& input);
    void Remove(); // intent only; submitted at the matching world frame
    void Service(const WorldFrame& frame);
    const NpcCreationStatus& Status() const;
private:
    struct State;
    State* state_;
};
}
