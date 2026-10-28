#pragma once
#include "../../Export.hpp"
#include "EffectControl.hpp"
#include "EffectInputs.hpp"
#include "EffectActions.hpp"
#include "EffectMutex.hpp"
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
    // Unrooted mapped path. Retain it before further managed calls; use
    // EffectPathMode::Resolved to submit that same path after admission checks.
    ObjectResult MapPath(void* caster,void* rooted_path) const;
    // Read-only official mutex queries; candidates restrict subsequent replay
    // to already-owned receipts. Callers retain entity/path through this call.
    // Non-null caster maps the path as the source AttackData check does. The
    // public-config check passes null and uses the original configured key.
    EffectMutexDecision HitMutex(void* caster,void* target,void* path,const EffectMutexRule&,float delay,
        const std::vector<EffectReference>& replay_candidates={}) const;
    // Throws on an unread configuration; absent map entry is a valid value.
    HitEffectRules ReadHitRules(const std::string& path) const;
    EffectControlResult SetProgress(EffectReference,float progress,float transition_seconds=0) const;
    EffectControlResult SetAnimatorState(EffectReference,int state_hash,
        float normalized_transition_duration=0,bool restart=true) const;
    EffectControlResult SetAnimatorState(EffectReference,const std::string& state,
        float normalized_transition_duration=0,bool restart=true) const;
    EffectControlResult SetAnimatorTrigger(EffectReference,const std::string& name,bool reset=false) const;
    EffectControlResult SetAnimatorParameter(EffectReference,const EffectAnimatorParameter&) const;
private:
    struct State;
    State* state_;
};
}
