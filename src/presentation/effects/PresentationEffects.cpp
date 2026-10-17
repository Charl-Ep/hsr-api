#include <hksr/presentation/effects/PresentationEffects.hpp>
#include <hksr/presentation/effects/OwnedEffectRequest.hpp>

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
}
