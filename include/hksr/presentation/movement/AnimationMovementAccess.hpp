#pragma once
#include "AnimationMovementData.hpp"

namespace hksr::sdk_v1 {
struct AnimationMovementAccessBinding {
    Method submit,clear;
    std::size_t owner=0,transform=0,animator_source=0,other_motion=0,data=0,attached=0,active=0,index=0;
    AnimationMoveDataLayout graph;
    Method position;
};
HKSR_API AnimationMovementAccessBinding ResolveAnimationMovementAccess460(RuntimeMetadata&);
HKSR_API AnimationMovementAccessBinding ResolveAnimationMovementAccess460(MethodResolver&,
    const std::vector<DeclaredField>& component,AnimationMoveDataLayout graph);
struct AnimationMovementObservation {
    CallResult read;
    void* owner=nullptr;void* transform=nullptr;void* animator_source=nullptr;
    void* other_motion=nullptr;void* data=nullptr;
    bool attached=false,active=false;
    std::int32_t segment_index=0;
};
struct AnimationMoveClockObservation {
    CallResult read;
    std::int32_t key=0;
    float animation_time=0,movement_time=0;
};
enum class MovementWriteState { NotAttempted, Owned, Detached, Replaced, Failed };
struct MovementWriteResult {
    MovementWriteState state=MovementWriteState::NotAttempted;
    CallResult call;
    bool attempted=false;
    AnimationMovementObservation before,after;
    std::string error;
};
// Engine-thread adapter. The caller retains the component and supplies a
// non-mutating predicate for its owned entity generation/world/model. Reads
// and eligibility failures do not call either native write. No automatic retry.
// Detached is an ownership observation, never a claim of natural completion.
class AnimationMovementAccess {
public:
    AnimationMovementAccess(AnimationMovementAccessBinding binding,GameEntity* owner):binding_(binding),owner_(owner){}
    HKSR_API AnimationMovementObservation Observe(void* component) const;
    HKSR_API AnimationMoveClockObservation Clock(const AnimationMovementData&) const;
    HKSR_API MovementWriteResult Submit(void* component,const AnimationMovementData&,
        const std::function<bool()>& current) const;
    HKSR_API MovementWriteResult Position(void* component,Position3,
        const std::function<bool()>& current) const;
    // Only clears this exact graph, leaves position and other motion unchanged.
    // This path is for the explicit-endpoint (mode 0) data built above; it does
    // not restore formation-derived facing or finish at the final endpoint.
    HKSR_API MovementWriteResult Interrupt(void* component,const AnimationMovementData&,
        const std::function<bool()>& current) const;
private:
    AnimationMovementAccessBinding binding_;
    GameEntity* owner_=nullptr;
};
}
