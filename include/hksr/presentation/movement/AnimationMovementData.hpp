#pragma once
#include "../../entity/MapNpcInputs.hpp"
#include "../../metadata/RuntimeMetadata.hpp"
#include <functional>
#include <memory>

namespace hksr::sdk_v1 {
enum class MovementClock { Animation, DistanceSpeed };
// Resolved world-space endpoints. Target selection, offsets, radius and return
// anchors belong to the caller; no battle target/formation context is evaluated.
struct AnimationMoveSegment {
    Position3 target;
    float start=0,end=1,speed=1,exit_time=-1;
    void* curve=nullptr; // optional retained, immutable official FloatCurve
};
struct AnimationMovePlan {
    Position3 origin;
    std::int32_t animation_key=0; // Animator.StringToHash, not a request ID
    MovementClock clock=MovementClock::Animation;
    bool force_start=false;
    std::vector<AnimationMoveSegment> segments;
};
struct CompiledMoveSegment {
    Position3 origin,target;
    float start=0,end=1,exit_time=-1;
    void* curve=nullptr;
};
struct CompiledAnimationMove {
    std::int32_t animation_key=0;
    float duration=0,end_time=-1;
    bool force_start=false;
    std::vector<CompiledMoveSegment> segments;
};
// No game calls. Speed mode derives the official distance-weighted intervals;
// animation mode preserves supplied normalized times, including times above 1.
HKSR_API CompiledAnimationMove CompileAnimationMove(const AnimationMovePlan&);

struct AnimationMoveDataLayout {
    std::size_t segments=0,previous_time=0,duration=0,end_time=0,key=0,progress=0,force_start=0;
    std::size_t curve=0,exit_time=0,origin=0,target_sample=0,segment_end=0,update_mode=0,segment_start=0;
};
struct AnimationMoveDataServices {
    ReferenceMethods references;
    AnimationMoveDataLayout layout;
    // Allocate fresh managed objects, without running constructors.
    std::function<ObjectResult()> data,segment,list;
    Method data_ctor,segment_ctor,list_ctor,list_add;
};
HKSR_API AnimationMoveDataServices ResolveAnimationMoveData460(RuntimeMetadata&);
HKSR_API AnimationMoveDataLayout ResolveAnimationMoveDataLayout460(
    const std::vector<DeclaredField>& data,const std::vector<DeclaredField>& segment,
    const std::vector<DeclaredField>& sample);

// Owns a detached FGNN graph. Does not submit motion or write to an entity,
// Animator, TaskContext or shared configuration. Every child stays rooted for
// the graph's lifetime. Close only after its component consumer has detached.
class AnimationMovementData {
public:
    HKSR_API explicit AnimationMovementData(AnimationMoveDataServices);
    HKSR_API ~AnimationMovementData(); // no engine calls
    AnimationMovementData(const AnimationMovementData&)=delete;
    AnimationMovementData& operator=(const AnimationMovementData&)=delete;
    HKSR_API bool Prepare(const AnimationMovePlan&);
    HKSR_API void* Object() const;
    HKSR_API const CompiledAnimationMove& Plan() const;
    HKSR_API const std::string& Error() const;
    HKSR_API bool Close();
private:
    friend class AnimationMovementAccess;
    bool BeginSubmission() const;
    struct State;std::unique_ptr<State> state_;
};
}
