#include <hksr/presentation/effects/PresentationEffects.hpp>
#include <hksr/presentation/effects/OwnedEffectRequest.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct PresentationEffects::State {EffectRequestServices services;EffectControlServices control;ConfigureEntityEffect configure_entity;EffectActionServices actions;};
PresentationEffects::PresentationEffects(EffectRequestServices services):PresentationEffects(std::move(services),{}){}
PresentationEffects::PresentationEffects(EffectRequestServices services,EffectControlServices control):PresentationEffects(std::move(services),std::move(control),{}){}
PresentationEffects::PresentationEffects(EffectRequestServices services,EffectControlServices control,ConfigureEntityEffect configure)
    :PresentationEffects(std::move(services),std::move(control),std::move(configure),{}){}
PresentationEffects::PresentationEffects(EffectRequestServices services,EffectControlServices control,ConfigureEntityEffect configure,EffectActionServices actions)
    :state_(new State{std::move(services),std::move(control),std::move(configure),std::move(actions)}){}
PresentationEffects::~PresentationEffects(){delete state_;}
std::unique_ptr<OwnedEffectRequest> PresentationEffects::CreateRequest(const std::string& prefix) const {
    auto services=state_->services;
    if(services.trace && !prefix.empty())
        services.trace=[sink=services.trace,prefix](const std::string& text){sink(prefix+text);};
    return std::make_unique<OwnedEffectRequest>(std::move(services),state_->configure_entity,state_->actions);
}
bool PresentationEffects::HasExitEvents() const{return static_cast<bool>(state_->services.exit_events);}
std::unique_ptr<EventRegistrationLease> PresentationEffects::RegisterExit(EntityToken owner,const std::string& state,
    const std::vector<EffectRemovalSource>& sources) const {
    return HasExitEvents()?state_->services.exit_events(owner,state,sources):nullptr;
}
void PresentationEffects::Trace(const std::string& text) const {
    if(state_->services.trace)state_->services.trace(text);
}
ObjectResult PresentationEffects::MapPath(void* caster,void* path) const {
    return state_->services.access.MapPath(caster,path);
}
EffectMutexDecision PresentationEffects::HitMutex(void* caster,void* target,void* path,const EffectMutexRule& rule,float delay,
    const std::vector<EffectReference>& candidates) const {
    if(!state_->services.hit_mutex)throw std::runtime_error("hit effect mutex service unavailable");
    return state_->services.hit_mutex(caster,target,path,rule,delay,candidates);
}
HitEffectRules PresentationEffects::ReadHitRules(const std::string& path) const {
    if(!state_->services.hit_rules)throw std::runtime_error("hit effect rules service unavailable");
    const auto value=state_->services.hit_rules(path);
    if(!value.available)throw std::runtime_error("hit effect rules unread: "+value.error);
    return value.value;
}
EffectControlResult PresentationEffects::SetProgress(EffectReference ref,float value,float duration) const {
    return state_->control.progress?state_->control.progress(ref,value,duration):
        EffectControlResult{EffectControlState::Unsupported,{},true,"effect progress control unavailable"};
}
EffectControlResult PresentationEffects::SetAnimatorState(EffectReference ref,int hash,float duration,bool restart) const {
    return state_->control.animator?state_->control.animator(ref,hash,duration,restart):
        EffectControlResult{EffectControlState::Unsupported,{},true,"effect Animator control unavailable"};
}
EffectControlResult PresentationEffects::SetAnimatorTrigger(EffectReference ref,const std::string& name,bool reset) const {
    return state_->actions.trigger?state_->actions.trigger(ref,name,reset):
        EffectControlResult{EffectControlState::Unsupported,{},true,"effect Animator trigger unavailable"};
}
EffectControlResult PresentationEffects::SetAnimatorParameter(EffectReference ref,const EffectAnimatorParameter& value) const {
    return state_->actions.parameter?state_->actions.parameter(ref,value):
        EffectControlResult{EffectControlState::Unsupported,{},true,"effect Animator parameter unavailable"};
}
EffectControlResult PresentationEffects::SetAnimatorState(EffectReference ref,const std::string& name,float duration,bool restart) const {
    return state_->control.named_animator?state_->control.named_animator(ref,name,duration,restart):
        EffectControlResult{EffectControlState::Unsupported,{},true,"named effect Animator control unavailable"};
}
}
