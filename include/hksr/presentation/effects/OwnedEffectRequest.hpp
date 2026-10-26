#pragma once
#include <hksr/presentation/effects/EffectAccess.hpp>
#include <hksr/presentation/effects/EffectCompletion.hpp>
#include <hksr/presentation/effects/EffectInputs.hpp>
#include <hksr/presentation/effects/EffectActions.hpp>
#include <hksr/presentation/effects/HitEffectControl.hpp>
#include <memory>
#include <hksr/presentation/events/EventRegistrationLease.hpp>
#include <hksr/animation/OfficialCatalog.hpp>

namespace hksr::sdk_v1 {
struct EffectRequestServices {
    api::ReferenceMethods references;
    api::EffectAccess access;
    api::EffectCompletionAllocation completion;
    std::function<api::ObjectResult()> manager;
    std::function<api::EffectInputResult(void*,void*,void*)> configure;
    std::function<void(const std::string&)> trace;
    // Explicit ownership BEFORE Submit (which may synchronously run plugins).
    // This is not inference from arbitrary official TargetEntity fields.
    std::function<void(api::EffectReference,void*)> observe_request;
    std::function<void(api::EffectReference)> retire_request;
    // Independent official event registration; no framework polling of effect
    // lists and no global/path-wide deletion. Returns a prepared/attached owner.
    std::function<std::unique_ptr<api::EventRegistrationLease>(api::EntityToken,const std::string&,const std::vector<api::EffectRemovalSource>&)> exit_events;
    ConfigureSpatialEffect configure_spatial;
    ConfigureImpactEffect configure_impact;
    std::function<api::EffectInputResult(void*,void*,void*,void*,const HitEffectPlacement&)> configure_hit;
    std::function<EffectControlResult(EffectReference,void*,Position3)> loaded_hit;
    std::function<EffectControlResult(void*,EffectReference,const HitEffectPlacement&)> update_hit;
    std::function<EffectControlResult(EffectReference,EffectMutexKind)> replay_hit;
    std::function<EffectMutexDecision(void*,void*,void*,const EffectMutexRule&,float,const std::vector<EffectReference>&)> hit_mutex;
    std::function<ConfigValue<HitEffectRules>(const std::string&)> hit_rules;
    std::function<Observed<bool>(EffectReference,void*,void*)> match_hit;
};
struct EffectRequestStatus {
    api::EffectReference reference;
    api::CallResult submit,release;
    api::Observed<bool> removal;
    api::EffectObservation observation;
    std::uint64_t completions=0,loaded_completions=0,foreign_completions=0;
    bool started=false,submitted=false,cancelled=false,request_ended=false,closed=false;
    unsigned removal_attempts=0;
    std::string detail;
    std::optional<EffectControlResult> hit_completion;
};
// One request, not a pool or an entity controller. Start/Service run on the
// engine thread; Cancel is data-only. Retains the original service as well as
// request inputs. The caller keeps this owner serviced after action changes.
// Destruction disconnects callbacks, but does NOT perform engine cleanup.
class HKSR_API OwnedEffectRequest {
public:
    explicit OwnedEffectRequest(EffectRequestServices services);
    OwnedEffectRequest(EffectRequestServices,ConfigureEntityEffect);
    OwnedEffectRequest(EffectRequestServices,ConfigureEntityEffect,EffectActionServices);
    ~OwnedEffectRequest();
    OwnedEffectRequest(const OwnedEffectRequest&)=delete;
    OwnedEffectRequest& operator=(const OwnedEffectRequest&)=delete;
    void Start(void* entity,void* path);
    void StartEntity(void* caster,void* target,void* path,const EntityEffectOptions&);
    void StartSpatial(void* caster,void* path,const SpatialEffectOptions&);
    void StartImpact(void* caster,void* target,void* path,const ImpactEffectOptions&);
    void StartHit(void* caster,void* target,void* path,const HitEffectPlacement&);
    EffectControlResult UpdateHit(const HitEffectPlacement&);
    EffectControlResult ReplayHit(EffectMutexKind);
    Observed<bool> MatchesHitPath(void* caster,void* path) const;
    // Track a receipt captured at an official submission boundary. The caller
    // must establish ownership and retain both inputs through this call.
    // Does not submit again, alter inputs, or replace official callbacks.
    // submit/completion counters describe Start only; Observe supplies the
    // state of a tracked request. Call Service after the original stack returns.
    // false means retention failed: the caller still owns the receipt. Service
    // then drains only this object's partial roots, never touches the request.
    bool TrackSubmitted(void* retained_manager,api::EffectReference);
    void Cancel(bool fade=true);
    void Cancel(EffectRemovalOptions);
    void Service();
    const EffectRequestStatus& Status() const;
private:
    void StartImpl(void* caster,void* target,void* path,const EntityEffectOptions*,const SpatialEffectOptions* = nullptr,const ImpactEffectOptions* = nullptr,const HitEffectPlacement* = nullptr);
    struct State;State* state_;
};
}
