#pragma once
#include "../world/WorldPlacement.hpp"
#include "../entity/EntityAccess.hpp"
#include <memory>
#include <atomic>
#include <optional>

namespace hksr::sdk_v1 {
struct ActorControlMethods {
    Method flush,place,mono,look_enabled,set_look_enabled,head,look,follow,stop,point;
    std::vector<std::string> binding_errors;
    Method model_attach_point;
    Method forbid_ground,get_forbid_ground;
    Method navigation_only,get_navigation_only,navigation_ground,get_navigation_ground;
    Method ik_head,ik_follow,ik_stop;
    Method ik_active,behaviour_enabled,set_behaviour_enabled;
    Method yaw,turn;
};
HKSR_API ActorControlMethods ResolveActorControlMethods(MethodResolver& source);
// Horizontal shortest-arc turn. Coincident points preserve the current yaw;
// the explicit offset still applies. Ratio follows the official [0,1] clamp.
HKSR_API float ResolveFacingYaw(Position3 origin,Position3 target,float current,float ratio,float offset);
struct PlacementWrite {
    EntityToken owner;void* controller=nullptr;Position3 position;Rotation4 rotation;
    unsigned long thread=0;std::atomic<bool> cancelled{false};bool completed=false;CallResult call;
    std::optional<bool> snap_to_ground;
    std::optional<bool> navigation_only,navigation_ground;
};
// The only write occurs after this controller's original movement flush.
// Requests/receipts belong to the official thread; cancellation precedes owner
// retirement. The monitor holds weak native requests, not game object ownership.
HKSR_API void InstallMovementPlacement(const ActorControlMethods& methods);
HKSR_API void RemoveMovementPlacementAtQuiescentPoint();
HKSR_API std::shared_ptr<PlacementWrite> QueueMovementPlacement(EntityToken owner,void* controller,Position3 position,Rotation4 rotation,std::optional<bool> snap_to_ground={});
class ActorControl {
public:
    HKSR_API explicit ActorControl(ActorControlMethods methods);
    HKSR_API ~ActorControl();
    HKSR_API ActorControl(const ActorControl&) noexcept;
    HKSR_API ActorControl& operator=(const ActorControl&) noexcept;
    HKSR_API ObjectResult Mono(void* controller) const;
    HKSR_API ObjectResult Head(void* mono) const;
    HKSR_API ObjectResult ModelHead(void* model,void* rooted_head_name) const;
    HKSR_API ObjectResult Look(void* mono) const;
    HKSR_API ObjectResult IkHead(void* look) const;
    HKSR_API CallResult FollowIk(void* look,void* transform) const;
    HKSR_API CallResult StopIk(void* look) const;
    HKSR_API Observed<bool> IkActive(void* look) const;
    HKSR_API Observed<bool> BehaviourEnabled(void* behaviour) const;
    HKSR_API CallResult BehaviourEnabled(void* behaviour,bool enabled) const;
    HKSR_API Observed<bool> Automatic(void* driver) const;
    HKSR_API CallResult Automatic(void* driver,bool value) const;
    HKSR_API Observed<bool> ForbidGround(void* controller) const;
    HKSR_API CallResult ForbidGround(void* controller,bool value) const;
    HKSR_API Observed<bool> NavigationOnly(void* controller) const;
    HKSR_API CallResult NavigationOnly(void* controller,bool value) const;
    HKSR_API Observed<bool> NavigationGround(void* controller) const;
    HKSR_API CallResult NavigationGround(void* controller,bool value) const;
    HKSR_API CallResult Follow(void* mono,void* transform) const;
    HKSR_API CallResult Stop(void* mono) const;
    HKSR_API CallResult Point(void* transform,Position3 point) const;
    HKSR_API Observed<float> Yaw(void* transform) const;
    // Submission only. The Transform component performs smoothing on its own
    // update; duration <= 0 applies immediately. No formation synchronization.
    HKSR_API CallResult Turn(void* transform,float yaw,float duration) const;
    HKSR_API void BindMono(ObjectMetadata& metadata,void* mono);
private: std::shared_ptr<const ActorControlMethods> methods_;
};
}
