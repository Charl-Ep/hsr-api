#include <hksr/presentation/effects/PresentationEffects.hpp>
#include <hksr/presentation/effects/OwnedEffectRequest.hpp>

namespace hksr::sdk_v1 {
struct PresentationEffects::State {EffectRequestServices services;};
PresentationEffects::PresentationEffects(EffectRequestServices services):state_(new State{std::move(services)}){}
PresentationEffects::~PresentationEffects(){delete state_;}
std::unique_ptr<OwnedEffectRequest> PresentationEffects::CreateRequest(const std::string& prefix) const {
    auto services=state_->services;
    if(services.trace && !prefix.empty())
        services.trace=[sink=services.trace,prefix](const std::string& text){sink(prefix+text);};
    return std::make_unique<OwnedEffectRequest>(std::move(services));
}
bool PresentationEffects::HasExitEvents() const{return static_cast<bool>(state_->services.exit_events);}
std::unique_ptr<EventRegistrationLease> PresentationEffects::RegisterExit(EntityToken owner,const std::string& state,
    const std::vector<EffectRemovalSource>& sources) const {
    return HasExitEvents()?state_->services.exit_events(owner,state,sources):nullptr;
}
void PresentationEffects::Trace(const std::string& text) const {
    if(state_->services.trace)state_->services.trace(text);
}
}
