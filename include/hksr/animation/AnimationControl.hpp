#pragma once
#include <memory>
#include "../managed/References.hpp"
#include "../metadata/RuntimeMetadata.hpp"
#include <string>
#include <vector>

namespace hksr::sdk_v1 {
struct BlinkControlMethods {
    Method set_source,source_value,source_active;
    std::size_t mask_offset=0;
    bool mask_available=false;
};
struct BlinkSourceObservation {Observed<bool> active,enabled;};
struct AnimationControlMethods {
    Method hash,has_state,play,npc_motion,free_style_state,controller;
    Method replace_path,replace_object,override_manager;
    std::vector<std::string> binding_errors;
    Method state_info,in_transition;
    Method layer_count,cross_fade;
    Method layer_index,layer_weight,set_layer_weight,update;
    BlinkControlMethods blink;
    Method ready_parameter_hash,float_get,float_set,parameter_count,parameter_at,parameter_hash,parameter_type;
    Method resolve_hash;
};
struct AnimatorStateObservation { CallResult read; int short_name_hash=0; float normalized_time=0; int full_path_hash=0; bool loop=false; };
// Thin official calls. The caller owns the current entity/model epoch and all
// input roots; this object neither acquires gameplay control nor ticks itself.
class AnimationControl {
public:
    HKSR_API explicit AnimationControl(AnimationControlMethods methods);
    HKSR_API ~AnimationControl();
    HKSR_API AnimationControl(const AnimationControl&) noexcept;
    HKSR_API AnimationControl& operator=(const AnimationControl&) noexcept;
    HKSR_API Observed<int> StateHash(void* rooted_name) const;
    // Borrowed managed text from this Animator's own name table; retain before
    // further managed calls. No controller enumeration or guessed names.
    HKSR_API ObjectResult ResolveStateName(void* animator,int hash) const;
    HKSR_API Observed<bool> HasState(void* animator,int layer,int hash) const;
    HKSR_API ObjectResult Controller(void* animator) const;
    HKSR_API CallResult PlayAnimator(void* animator,int hash,int layer,float normalized_time) const;
    HKSR_API CallResult PlayNpcMotion(void* free_style,unsigned id) const;
    HKSR_API CallResult PlayFreeStyleState(void* free_style,int hash,float first,float second) const;
    // Null path removes the official default-key override. It is not an
    // instruction to erase every entry in AnimatorOverrideManager.
    HKSR_API CallResult ReplacePath(void* model,void* rooted_path) const;
    HKSR_API ObjectResult OverrideManager(void* loader) const;
    HKSR_API CallResult ReplaceObject(void* manager,void* rooted_controller,void* rooted_own_tag) const;
    // 4.5.0 retained ABI: StateInfoIndex 0, 36-byte output, hash at 0/time at12.
    // Interpretation lives in the adapter; this is not a universal Unity ABI.
    HKSR_API AnimatorStateObservation CurrentState(void* animator,int layer=0) const;
    HKSR_API AnimatorStateObservation NextState(void* animator,int layer=0) const;
    HKSR_API Observed<bool> InTransition(void* animator,int layer=0) const;
    HKSR_API Observed<int> LayerCount(void* animator) const;
    HKSR_API Observed<int> LayerIndex(void* animator,void* rooted_name) const;
    HKSR_API Observed<float> LayerWeight(void* animator,int layer) const;
    HKSR_API CallResult SetLayerWeight(void* animator,int layer,float weight) const;
    // Explicit evaluation can dispatch events even at zero delta. Caller must
    // recheck entity/model/intent ownership before issuing subsequent writes.
    HKSR_API CallResult Evaluate(void* animator,float delta_time) const;
    HKSR_API CallResult CrossFade(void* animator,int hash,float normalized_duration,int layer,float normalized_time=0) const;
    HKSR_API BlinkSourceObservation BlinkSource(void* emotion_component,int source) const;
    HKSR_API CallResult SetBlinkSource(void* emotion_component,int source,bool enabled) const;
    // Current official ready consumer uses BuildInAnimParam value 2 (Int16).
    // Query its hash through the game; never use Animator.speed.
    HKSR_API Observed<int> ReadyTransitParameter() const;
    HKSR_API Observed<bool> HasFloatParameter(void* animator,int hash) const;
    HKSR_API Observed<float> FloatParameter(void* animator,int hash) const;
    HKSR_API CallResult SetFloatParameter(void* animator,int hash,float value) const;
private:
    std::shared_ptr<const AnimationControlMethods> methods_;
};
HKSR_API AnimationControlMethods ResolveAnimationControlMethods(MethodResolver& resolver);
HKSR_API void ResolveBlinkControl(AnimationControlMethods& methods,MethodResolver& resolver,
    const std::vector<DeclaredField>& emotion_fields);
// Current declarations, not retained numeric offsets. Resolve each owner type
// separately so a failure does not disable unrelated animation operations.
struct ControllerLinkField {
    std::size_t offset=0;
    bool available=false;
    std::string error;
};
HKSR_API ControllerLinkField ResolveModelControllerLoader(const std::vector<DeclaredField>& fields);
HKSR_API ControllerLinkField ResolveControllerLoaderModel(const std::vector<DeclaredField>& fields);
HKSR_API ControllerLinkField ResolveControllerLoaderManager(const std::vector<DeclaredField>& fields);
HKSR_API ObjectResult ReadControllerLink(void* rooted_object,const ControllerLinkField& field);
}
