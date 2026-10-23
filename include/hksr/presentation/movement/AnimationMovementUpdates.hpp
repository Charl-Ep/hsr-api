#pragma once
#include "AnimationMovementAccess.hpp"

namespace hksr::sdk_v1 {
HKSR_API Method ResolveAnimationMovementUpdate460(MethodResolver&);
struct AnimationMovementUpdateStatus {
    bool observed=false,started=false,completed=false,interrupted=false,replaced=false,failed=false;
    float animation_time=0,movement_time=0;
    std::uint64_t updates=0;
};
// Read-only observation of the official evaluator, for one component/graph.
// Does not alter arguments, outputs, timing, exceptions or return values.
// Construct before submitting the graph. Poll on the registering thread;
// Close disconnects only. The caller retains the component and graph.
class AnimationMovementUpdates {
public:
    HKSR_API AnimationMovementUpdates(Method update,AnimationMovementAccessBinding,
        GameEntity* owner,void* component,void* graph);
    HKSR_API ~AnimationMovementUpdates();
    AnimationMovementUpdates(const AnimationMovementUpdates&)=delete;
    AnimationMovementUpdates& operator=(const AnimationMovementUpdates&)=delete;
    HKSR_API AnimationMovementUpdateStatus Status() const;
    HKSR_API void Close() noexcept;
private:
    struct State;State* state_=nullptr;
};
}
