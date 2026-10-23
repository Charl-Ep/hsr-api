#pragma once
#include "AnimationMovementAccess.hpp"
#include "../../entity/EntityLifecycle.hpp"
#include "../../managed/ManagedRoots.hpp"

namespace hksr::sdk_v1 {
// All callbacks run synchronously on the entity owner thread. Registration
// must use GameEntity.CreateComponent; initialization includes model catch-up.
// lookup and observe remain reads. remove must destroy exactly the supplied
// component through the official registry, not unlink native arrays directly.
struct MovementHostServices {
    ReferenceMethods references;
    std::function<bool()> current,alive;
    std::function<ObjectResult()> lookup,create;
    std::function<CallResult(void*)> initialize,remove;
    std::function<AnimationMovementObservation(void*)> observe;
    std::function<Observed<bool>()> read_root_motion,read_pending_motion;
    std::function<CallResult(bool)> write_root_motion;
    std::function<Observed<bool>(void*)> read_callback;
    std::function<CallResult(void*,bool)> write_callback;
    std::vector<void*> retained_inputs;
    std::function<void(const std::string&)> trace;
};
struct MovementHostStatus {
    bool attempted=false,created=false,prepared=false,waiting=false,closed=false;
    bool owner_lost=false,component_replaced=false;
    CallResult creation,initialization,removal,root_pause,root_restore,callback_pause,callback_restore;
    std::string error;
};
// An exclusive motion lease for one owned actor. It suppresses ordinary root
// translation, leaving animation/IK callbacks running, and waits for already
// submitted Adventure motion to drain before a graph/effect takes control.
// Close only after all request-owned movement channels have detached.
class HKSR_API OwnedMovementHost {
public:
    OwnedMovementHost(MovementHostServices,EntityToken);
    ~OwnedMovementHost(); // no engine calls
    OwnedMovementHost(const OwnedMovementHost&)=delete;
    OwnedMovementHost& operator=(const OwnedMovementHost&)=delete;
    bool Prepare();
    bool Ready();
    bool Close();
    void* Component() const;
    const MovementHostStatus& Status() const;
private:
    struct State;State* state_;
};
}
