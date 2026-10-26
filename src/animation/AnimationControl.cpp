#include <hksr/animation/AnimationControl.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <cmath>

namespace hksr::sdk_v1 {
AnimationControl::AnimationControl(AnimationControlMethods methods):methods_(std::make_shared<const AnimationControlMethods>(std::move(methods))){}
AnimationControl::~AnimationControl()=default;
AnimationControl::AnimationControl(const AnimationControl&) noexcept=default;
AnimationControl& AnimationControl::operator=(const AnimationControl&) noexcept=default;

ObjectResult AnimationControl::ResolveStateName(void* animator,int hash) const {
    ObjectResult out;if(animator)out.call=native::Call(methods_->resolve_hash,&out.object,animator,hash);return out;
}
ObjectResult AnimationControl::LayerName(void* animator,int layer) const {
    ObjectResult out;if(animator && layer>=0)out.call=native::Call(methods_->layer_name,&out.object,animator,layer);return out;
}
Observed<float> AnimationControl::HitMotionHeight(void* motion) const {
    Observed<float> out;float clock=0;
    if(!motion || !methods_->hit_motion.clock_available ||
        !native::CopyMemoryChecked(&clock,static_cast<const unsigned char*>(motion)+methods_->hit_motion.clock_offset,sizeof(clock)) ||
        !std::isfinite(clock))return out;
    out.read=native::Call(methods_->hit_motion.flight_height,&out.value,motion,clock);
    if(!out.read.returned || !std::isfinite(out.value)){out.read.returned=false;return out;}
    if(out.value<=0)out.read=native::Call(methods_->hit_motion.fall_height,&out.value,motion);
    if(!std::isfinite(out.value))out.read.returned=false;
    return out;
}
Observed<int> AnimationControl::ReadyTransitParameter() const {
    Observed<int> out;out.read=native::Call(methods_->ready_parameter_hash,&out.value,static_cast<std::int16_t>(2));return out;
}
Observed<int> AnimationControl::HitAngleParameter() const {
    Observed<int> out;out.read=native::Call(methods_->ready_parameter_hash,&out.value,static_cast<std::int16_t>(1));return out;
}
Observed<bool> AnimationControl::HasFloatParameter(void* animator,int hash) const {
    Observed<bool> out;int count=0;
    if(!animator)return out;
    out.read=native::Call(methods_->parameter_count,&count,animator);
    if(!out.read.returned || count<0){out.read.returned=false;return out;}
    for(int i=0;i<count;++i) {
        void* parameter=nullptr;out.read=native::Call(methods_->parameter_at,&parameter,animator,i);
        if(!out.read.returned || !parameter){out.read.returned=false;return out;}
        int name=0;out.read=native::Call(methods_->parameter_hash,&name,parameter);
        if(!out.read.returned)return out;
        if(name!=hash)continue;
        int type=0;out.read=native::Call(methods_->parameter_type,&type,parameter);
        if(out.read.returned)out.value=type==1; // Unity AnimatorControllerParameterType.Float
        return out;
    }
    return out;
}
Observed<float> AnimationControl::FloatParameter(void* animator,int hash) const {
    Observed<float> out;if(animator)out.read=native::Call(methods_->float_get,&out.value,animator,hash);return out;
}
CallResult AnimationControl::SetFloatParameter(void* animator,int hash,float value) const {
    return animator?native::CallVoid(methods_->float_set,animator,hash,value):CallResult{};
}
BlinkSourceObservation AnimationControl::BlinkSource(void* emotion,int source) const {
    BlinkSourceObservation out;void* mask=nullptr;
    if(!emotion || !methods_->blink.mask_available || source<0)return out;
    if(!native::CopyMemoryChecked(&mask,static_cast<const unsigned char*>(emotion)+methods_->blink.mask_offset,sizeof(mask)) || !mask)return out;
    out.active.read=native::Call(methods_->blink.source_active,&out.active.value,mask,source);
    out.enabled.read=native::Call(methods_->blink.source_value,&out.enabled.value,mask,source);
    return out;
}
CallResult AnimationControl::SetBlinkSource(void* emotion,int source,bool enabled) const {
    return emotion && source>=0?native::CallVoid(methods_->blink.set_source,emotion,source,enabled):CallResult{};
}
Observed<int> AnimationControl::LayerCount(void* animator) const {
    Observed<int> out;if(animator)out.read=native::Call(methods_->layer_count,&out.value,animator);return out;
}
Observed<int> AnimationControl::LayerIndex(void* animator,void* name) const {
    Observed<int> out;if(animator && name)out.read=native::Call(methods_->layer_index,&out.value,animator,name);return out;
}
Observed<float> AnimationControl::LayerWeight(void* animator,int layer) const {
    Observed<float> out;if(animator && layer>=0)out.read=native::Call(methods_->layer_weight,&out.value,animator,layer);return out;
}
CallResult AnimationControl::SetLayerWeight(void* animator,int layer,float weight) const {
    return animator && layer>=0?native::CallVoid(methods_->set_layer_weight,animator,layer,weight):CallResult{};
}
CallResult AnimationControl::Evaluate(void* animator,float delta) const {
    return animator?native::CallVoid(methods_->update,animator,delta):CallResult{};
}
CallResult AnimationControl::CrossFade(void* animator,int hash,float duration,int layer,float time) const {
    return animator?native::CallVoid(methods_->cross_fade,animator,hash,duration,layer,time,0.0f):CallResult{};
}
Observed<bool> AnimationControl::InTransition(void* animator,int layer) const {
    Observed<bool> out;if(animator) out.read=native::Call(methods_->in_transition,&out.value,animator,layer);return out;
}
ObjectResult ReadControllerLink(void* object,const ControllerLinkField& field) {
    ObjectResult out;
    if(object && field.available) out.call.returned=native::CopyMemoryChecked(&out.object,
        static_cast<const unsigned char*>(object)+field.offset,sizeof(out.object));
    return out;
}
Observed<int> AnimationControl::StateHash(void* name) const {
    Observed<int> out;
    if(name) out.read=native::Call(methods_->hash,&out.value,name);
    return out;
}
Observed<bool> AnimationControl::HasState(void* animator,int layer,int hash) const {
    Observed<bool> out;
    if(animator) out.read=native::Call(methods_->has_state,&out.value,animator,layer,hash);
    return out;
}
ObjectResult AnimationControl::Controller(void* animator) const {
    ObjectResult out;
    if(animator) out.call=native::Call(methods_->controller,&out.object,animator);
    return out;
}
CallResult AnimationControl::PlayAnimator(void* animator,int hash,int layer,float time) const {
    return animator?native::CallVoid(methods_->play,animator,hash,layer,time):CallResult{};
}
CallResult AnimationControl::PlayNpcMotion(void* free_style,unsigned id) const {
    return free_style?native::CallVoid(methods_->npc_motion,free_style,id):CallResult{};
}
CallResult AnimationControl::PlayFreeStyleState(void* free_style,int hash,float first,float second) const {
    return free_style?native::CallVoid(methods_->free_style_state,free_style,hash,first,second):CallResult{};
}
CallResult AnimationControl::ReplacePath(void* model,void* path) const {
    return model?native::CallVoid(methods_->replace_path,model,path,static_cast<void*>(nullptr)):CallResult{};
}
ObjectResult AnimationControl::OverrideManager(void* loader) const {
    ObjectResult out;
    if(loader) out.call=native::Call(methods_->override_manager,&out.object,loader);
    return out;
}
CallResult AnimationControl::ReplaceObject(void* manager,void* controller,void* tag) const {
    return manager && tag?native::CallVoid(methods_->replace_object,manager,controller,tag):CallResult{};
}
}
