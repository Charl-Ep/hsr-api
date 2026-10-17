#pragma once
#include "../../Export.hpp"
#include "EffectControl.hpp"
#include "EffectInputs.hpp"
#include "EffectActions.hpp"
#include <memory>
#include <string>
#include <vector>

namespace hksr::sdk_v1 {
struct EffectRequestServices;
struct EntityToken;
struct EffectRemovalSource;
class OwnedEffectRequest;
class EventRegistrationLease;

// Native capability bundle. Returned requests/registrations own their engine
// cleanup; this service does not submit, cancel or retire them implicitly.
class HKSR_API PresentationEffects {
public:
    explicit PresentationEffects(EffectRequestServices); // explicit low-level provider
    PresentationEffects(EffectRequestServices,EffectControlServices);
    PresentationEffects(EffectRequestServices,EffectControlServices,ConfigureEntityEffect);
    PresentationEffects(EffectRequestServices,EffectControlServices,ConfigureEntityEffect,EffectActionServices);
    ~PresentationEffects();
    PresentationEffects(const PresentationEffects&)=delete;
    PresentationEffects& operator=(const PresentationEffects&)=delete;
    std::unique_ptr<OwnedEffectRequest> CreateRequest(const std::string& trace_prefix={}) const;
    bool HasExitEvents() const;
    std::unique_ptr<EventRegistrationLease> RegisterExit(EntityToken,const std::string& state,
        const std::vector<EffectRemovalSource>&) const;
    void Trace(const std::string&) const;
    EffectControlResult SetProgress(EffectReference,float progress,float transition_seconds=0) const;
    EffectControlResult SetAnimatorState(EffectReference,int state_hash,
        float normalized_transition_duration=0,bool restart=true) const;
    EffectControlResult SetAnimatorTrigger(EffectReference,const std::string& name,bool reset=false) const;
private:
    struct State;
    State* state_;
};
}
