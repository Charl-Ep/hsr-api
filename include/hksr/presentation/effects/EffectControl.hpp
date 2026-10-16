#pragma once
#include "EffectAccess.hpp"
#include "../../metadata/RuntimeMetadata.hpp"
#include <functional>

namespace hksr::sdk_v1 {
enum class EffectControlState { Applied, NotLoaded, Ended, Unsupported, Failed };
struct EffectControlResult {
    EffectControlState state=EffectControlState::Failed;
    CallResult call;
    bool references_released=true;
    std::string detail;
};
struct EffectControlMethods {
    Method component,alive,progress_behavior,progress_owner,progress_set,animator_play,animator_cross_fade;
    std::vector<std::string> binding_errors;
};
struct EffectControlServices {
    std::function<EffectControlResult(EffectReference,float,float)> progress;
    std::function<EffectControlResult(EffectReference,int,float,bool)> animator;
};
// Engine-thread operations on a retained request, including one already fading.
// Never searches the global manager, forces a load, ticks a plugin, or queues a
// retry. Applied means the setter returned, not that a rendered frame changed.
// Returned component references are retained only during an operation. Close
// releases cached type references; destruction itself makes no engine calls.
class HKSR_API EffectControl {
public:
    EffectControl(EffectAccess,EffectControlMethods,ReferenceMethods,ManagedObjects&,
        const std::vector<DeclaredField>& mono_effect_fields);
    ~EffectControl();
    EffectControl(const EffectControl&)=delete;
    EffectControl& operator=(const EffectControl&)=delete;
    EffectControlResult SetProgress(EffectReference,float progress,float transition_seconds=0);
    // First Animator on the effect's GameObject, layer zero, as used by the
    // SetEffectAnimatorState task. Not the all-Animators behavior overload.
    EffectControlResult SetAnimatorState(EffectReference,int state_hash,
        float normalized_transition_duration=0,bool restart=true);
    bool Close();
private:
    struct State;State* state_;
};
HKSR_API EffectControlMethods ResolveEffectControl460(MethodResolver&);
}
