#pragma once
#include "AnimationMovementUpdates.hpp"
#include "OwnedMovementHost.hpp"
#include "../../entity/EntityLifecycle.hpp"

namespace hksr::sdk_v1 {
struct MovementRequestServices {
    AnimationMoveDataServices data;
    AnimationMovementAccessBinding access;
    Method update;
    std::vector<void*> model_objects; // identities used by current; retained through cleanup
    EntityLifecycle* lifecycle=nullptr;
    // current: this client's captured world/model is usable for submission.
    // alive: the entity lifetime has not ended; true during BeforeDispose.
    // Both run on the owner thread and must not perform game writes.
    std::function<bool()> current,alive;
    std::shared_ptr<OwnedMovementHost> host;
    std::function<Position3()> origin; // sample after pending ordinary motion drains
};
struct MovementRequestStatus {
    bool attempted=false,accepted=false,started=false,completed=false,cancelled=false,closed=false;
    bool interrupted=false,replaced=false,owner_lost=false;
    bool waiting_for_host=false;
    float animation_time=0,movement_time=0;
    MovementWriteResult submission,interruption;
    std::string error;
};
// One private animated or instant movement on an owned entity. Retains graph and
// component through native evaluation; cancels at entity pre-disposal. Model
// handoff callers cancel and drain before replacing the model. Service detects
// unexpected model changes and clears only this graph, never a successor.
class OwnedMovementRequest {
public:
    HKSR_API OwnedMovementRequest(MovementRequestServices,EntityToken,void* component);
    HKSR_API ~OwnedMovementRequest(); // disconnects; no engine calls
    OwnedMovementRequest(const OwnedMovementRequest&)=delete;
    OwnedMovementRequest& operator=(const OwnedMovementRequest&)=delete;
    HKSR_API void Start(const AnimationMovePlan&);
    HKSR_API void StartPosition(Position3);
    HKSR_API void Cancel(); // data-only; Service performs the scoped clear
    HKSR_API void Service();
    HKSR_API const MovementRequestStatus& Status() const;
private:
    struct State;std::shared_ptr<State> state_;
    void Begin();
};
}
