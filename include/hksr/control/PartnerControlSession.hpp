#pragma once
#include "../entity/EntityOwner.hpp"
#include "../entity/EntityAccess.hpp"
#include "../managed/ManagedObjects.hpp"

namespace hksr::sdk_v1 {
enum class VisibilityFollow : std::uint16_t {
    None=0, Visible=1, Invisible=2, Follow=3,
    TransmitVisible=4, TransmitInvisible=8, Transmit=12
};
struct PartnerAttachment {
    EntityToken target;
    std::string self_point, target_point;
    bool disable_when_invisible=false, use_target_scale=false;
    VisibilityFollow visibility=VisibilityFollow::None;
};
struct PartnerVisibility {
    std::string key;
    bool visible=true;
    bool mute_collider_when_hidden=false, mute_triggers_when_hidden=false, mute_audio_when_hidden=false;
};
struct PartnerIdleAnimation {
    std::string state;
    float normalized_start=0, normalized_transition=0, normalized_end=1;
};
struct PartnerControlResult {
    // Returned records native completion, not a visual acceptance result.
    bool returned=false, references_released=true;
    std::vector<CallResult> calls;
    std::string error;
};
struct PartnerControlMethods {
    Method component, owner_world, member_name, group_leader;
    Method attach, cancel_smooth_follow, animation_follow, visibility_follow;
    std::size_t component_owner=0;
    Method visibility, idle_animation;
};
// Immediate commands on an owned Partner. The engine's existing components
// schedule attachment, animation synchronization and visibility propagation.
// No animation ticking, entity creation, gameplay task or global selector runs
// here. Targets must be current entities owned by the same client/world.
// Use on the official owner thread after the Partner factory has returned.
// Close releases adapter references only; EntityCreationSession owns retirement
// of the actor and its official components, including their follow relations.
class HKSR_API PartnerControlSession {
public:
    PartnerControlSession(const EntityOwner&, EntityToken partner, EntityToken summoner,
        GameWorld*, PartnerControlMethods, ReferenceMethods, ManagedObjects&);
    ~PartnerControlSession();
    PartnerControlSession(const PartnerControlSession&)=delete;
    PartnerControlSession& operator=(const PartnerControlSession&)=delete;
    PartnerControlResult SetMemberName(const std::string&);
    // Uses official AttachTo's self/target attach-point contract, then disables
    // smooth following. Attachment strings must name actual model attach points.
    PartnerControlResult Attach(const PartnerAttachment&);
    PartnerControlResult FollowAnimation(EntityToken target={}); // empty clears
    PartnerControlResult FollowVisibility(EntityToken target, VisibilityFollow);
    // A nonempty per-entity key owns one hide request. visible=true removes
    // only that key; other hide requests still apply. Close does not show it.
    PartnerControlResult SetVisibility(const PartnerVisibility&);
    // Requests an explicit state through the Partner's initialized official
    // Idle logic state. Does not install a controller or run a gameplay task.
    // Returning records request submission, not completion of the animation.
    PartnerControlResult PlayIdle(const PartnerIdleAnimation&);
    bool Close();
private:
    struct State; State* state_;
};
HKSR_API PartnerControlMethods ResolvePartnerControl460(MethodResolver&,
    const std::vector<DeclaredField>& component_fields,
    const std::vector<DeclaredField>& attachment_fields,
    const std::vector<DeclaredField>& visibility_fields);
}
