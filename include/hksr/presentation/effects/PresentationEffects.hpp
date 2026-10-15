#pragma once
#include "../../Export.hpp"
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
    ~PresentationEffects();
    PresentationEffects(const PresentationEffects&)=delete;
    PresentationEffects& operator=(const PresentationEffects&)=delete;
    std::unique_ptr<OwnedEffectRequest> CreateRequest(const std::string& trace_prefix={}) const;
    bool HasExitEvents() const;
    std::unique_ptr<EventRegistrationLease> RegisterExit(EntityToken,const std::string& state,
        const std::vector<EffectRemovalSource>&) const;
    void Trace(const std::string&) const;
private:
    struct State;
    State* state_;
};
}
