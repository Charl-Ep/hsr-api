#include <hksr/presentation/effects/OwnedEffectRequest.hpp>
#include <mutex>
#include <stdexcept>

namespace hksr::sdk_v1 {
namespace {
// No borrowed MonoEffect pointer survives a callback. The parameter identity
// is compared only, never dereferenced there. Shared ownership also covers a
// dispatch already in flight when the native owner disconnects its bridge.
struct Signals {
    std::mutex mutex;
    void* parameters=nullptr;
    std::uint64_t completions=0,loaded=0,foreign=0;
    void Receive(api::EffectCompletionEvent event) {
        std::lock_guard<std::mutex> lock(mutex);
        if(event.parameters!=parameters){++foreign;return;}
        ++completions;if(event.effect)++loaded;
    }
};
}
struct OwnedEffectRequest::State {
    EffectRequestServices services;
    ConfigureEntityEffect configure_entity;
    EffectActionServices actions;
    EffectRemovalOptions removal_options;
    bool explicit_removal=false;
    std::uint64_t removal_revision=0,removed_revision=0;
    api::ManagedRoots roots;
    std::vector<api::ManagedRootId> handles;
    std::shared_ptr<Signals> signals=std::make_shared<Signals>();
    std::unique_ptr<api::EffectCompletion> callback;
    EffectRequestStatus status;
    void* manager=nullptr;
    bool busy=false,fade=true,release_attempted=false,source_observed=false;
    std::uint64_t removed_at_loaded=0;
    bool tracked=false,receipt_retained=false;
    void* last_observed_instance=nullptr; // identity comparison only, never dereferenced
    std::uint64_t observed_loads=0,removed_at_observation=0;
    explicit State(EffectRequestServices value):services(std::move(value)),roots(services.references){}
    void Trace(const std::string& text) const noexcept {
        if(services.trace)try{services.trace(text);}catch(...){}
    }
    void Keep(void* object) {
        if(!object)throw std::runtime_error("effect input is null");
        const auto id=roots.Keep(object);handles.push_back(id);
        if(!roots.Status(id).ready)throw std::runtime_error("effect input retention failed");
    }
    void ReadSignals() {
        std::lock_guard<std::mutex> lock(signals->mutex);
        status.completions=signals->completions;
        status.loaded_completions=signals->loaded;
        status.foreign_completions=signals->foreign;
    }
    void Drain() {
        if(source_observed) {
            // Only after official release/reuse, never on Cancel/Submit return.
            source_observed=false;
            try {
                if(services.retire_request)services.retire_request(status.reference);
            }catch(const std::exception& e){Trace(std::string("effect source retirement: ")+e.what());}
            // SDK retains any unresolved weak receipt; bookkeeping cleanup
            // must not block completed official cancellation or client shutdown.
        }
        const bool callback_closed=!callback || callback->Close();
        for(auto it=handles.begin();it!=handles.end();) {
            if(roots.Release(*it).complete){roots.Forget(*it);it=handles.erase(it);}
            else ++it;
        }
        status.closed=callback_closed && handles.empty();
    }
    void ObserveEnd() {
        auto& s=status;
        s.observation=services.access.Observe(s.reference);
        const auto& o=s.observation;
        // Missing reads are not evidence of a different pool generation.
        const bool reused=o.id_before.read.returned && o.id_after.read.returned &&
            o.id_before.value==o.id_after.value && o.id_after.value!=s.reference.id;
        if(reused || (o.same_generation && o.released.read.returned && o.released.value)) {
            s.request_ended=true;
            s.detail=reused?"official parameter pool advanced; no writes to reused input":"official parameters released";
        }
        if(tracked && !s.request_ended && o.same_generation && o.instance.call.returned) {
            // Official callbacks remain untouched. A newly observed instance
            // is evidence for retrying a previously unmatched cancellation.
            if(o.instance.object && o.instance.object!=last_observed_instance)++observed_loads;
            last_observed_instance=o.instance.object;
        }
    }
    void ReleaseUnsubmitted() {
        if(release_attempted)return;
        release_attempted=true;
        Trace("release unsubmitted begin id="+std::to_string(status.reference.id));
        status.release=services.access.ReleaseUnsubmitted(status.reference);
        Trace("release unsubmitted returned="+std::to_string(status.release.returned)+" exception="+std::to_string(status.release.exception));
        if(status.release.returned)status.request_ended=true;
        else status.detail="unsubmitted release unresolved; retained receipt, no repeated decrement";
    }
};
OwnedEffectRequest::OwnedEffectRequest(EffectRequestServices services):OwnedEffectRequest(std::move(services),{}){}
OwnedEffectRequest::OwnedEffectRequest(EffectRequestServices services,ConfigureEntityEffect configure)
    :OwnedEffectRequest(std::move(services),std::move(configure),{}){}
OwnedEffectRequest::OwnedEffectRequest(EffectRequestServices services,ConfigureEntityEffect configure,EffectActionServices actions)
    :state_(new State(std::move(services))) {
    state_->configure_entity=std::move(configure);state_->actions=std::move(actions);
}
OwnedEffectRequest::~OwnedEffectRequest(){delete state_;}
const EffectRequestStatus& OwnedEffectRequest::Status() const{return state_->status;}
void OwnedEffectRequest::Cancel(bool fade) {
    if(state_->fade!=fade)++state_->removal_revision;
    state_->status.cancelled=true;state_->fade=fade;
    // General scene teardown must not drop already requested immediate/unbind
    // semantics for a request whose asynchronous completion is still pending.
    state_->removal_options.fade=fade;
}
void OwnedEffectRequest::Cancel(EffectRemovalOptions options) {
    const auto previous=state_->removal_options;
    if(!state_->explicit_removal || previous.fade!=options.fade || previous.immediate_fade!=options.immediate_fade || previous.unbind!=options.unbind)
        ++state_->removal_revision;
    state_->status.cancelled=true;state_->fade=options.fade;
    state_->explicit_removal=true;state_->removal_options=options;
}
void OwnedEffectRequest::Start(void* entity,void* path) {
    StartImpl(entity,entity,path,nullptr);
}
void OwnedEffectRequest::StartEntity(void* caster,void* target,void* path,const EntityEffectOptions& options) {
    // Engine preparation may reenter the caller and replace its action data.
    const auto captured=options;
    StartImpl(caster,target,path,&captured);
}
void OwnedEffectRequest::StartSpatial(void* caster,void* path,const SpatialEffectOptions& options) {
    const auto captured=options;
    StartImpl(caster,nullptr,path,nullptr,&captured);
}
void OwnedEffectRequest::StartImpl(void* entity,void* target,void* path,const EntityEffectOptions* options,const SpatialEffectOptions* spatial) {
    auto& s=*state_;auto& out=s.status;
    if(out.started)throw std::logic_error("effect request already started");
    out.started=true;
    if(out.cancelled){out.request_ended=true;out.closed=true;return;}
    s.busy=true;
    struct Exit{bool& busy;~Exit(){busy=false;}} exit{s.busy};
    try {
        if(options && !s.configure_entity)throw std::runtime_error("entity effect configuration unavailable");
        if(spatial && !s.services.configure_spatial)throw std::runtime_error("spatial effect configuration unavailable");
        s.Keep(entity);s.Keep(path);
        if(target && target!=entity)s.Keep(target);
        if(!target && !spatial)throw std::runtime_error("entity effect target is null");
        s.Trace("prepare manager/path begin");
        const auto service=s.services.manager();
        if(!service.call.returned || !service.object)throw std::runtime_error("current effect manager unavailable");
        s.Keep(service.object);s.manager=service.object;
        const auto mapped=s.services.access.MapPath(entity,path);
        if(!mapped.call.returned || !mapped.object)throw std::runtime_error("effect path mapping failed");
        s.Keep(mapped.object);
        const auto acquired=s.services.access.Acquire(mapped.object);
        out.reference=acquired.reference; // retain footprint even on partial factory failure
        s.Trace("acquire returned="+std::to_string(acquired.call.returned)+" id="+std::to_string(out.reference.id));
        if(!acquired.call.returned || !out.reference.parameters)throw std::runtime_error("effect request acquisition failed");
        s.Keep(out.reference.parameters);
        s.signals->parameters=out.reference.parameters;
        s.callback=std::make_unique<api::EffectCompletion>(s.services.completion,s.services.references,
            [signals=s.signals](api::EffectCompletionEvent event){signals->Receive(event);});
        const auto callback=s.callback->Create();
        if(!callback.call.returned || !callback.object)throw std::runtime_error("effect completion creation failed");
        const auto configured=spatial?s.services.configure_spatial(out.reference.parameters,entity,callback.object,*spatial):
            options?s.configure_entity(out.reference.parameters,entity,target,callback.object,*options):
            s.services.configure(out.reference.parameters,entity,callback.object);
        s.Trace("configure target="+std::to_string(configured.target.returned)+" callback="+std::to_string(configured.completion.returned));
        if(!configured.target.returned || !configured.completion.returned)throw std::runtime_error("effect request configuration incomplete");
        if(out.cancelled){s.ReleaseUnsubmitted();}
        else {
            if(s.services.observe_request) {
                // Reserve retirement responsibility even if the observer
                // partially records this request and then throws.
                s.source_observed=true;
                s.services.observe_request(out.reference,entity);
            }
            // Ledger and signal owner exist BEFORE the potentially synchronous
            // official call. Failure may be a partial handoff, not no-op.
            out.submitted=true;
            s.Trace("submit begin id="+std::to_string(out.reference.id));
            out.submit=s.services.access.Submit(s.manager,out.reference);
            s.Trace("submit returned="+std::to_string(out.submit.returned)+" exception="+std::to_string(out.submit.exception));
            out.detail=out.submit.returned?"submitted; visibility not yet established":"submission unresolved; cancel via official manager";
            if(!out.submit.returned)out.cancelled=true;
        }
    }catch(const std::exception& e) {
        out.detail=e.what();out.cancelled=true;
        s.Trace("preparation failure: "+out.detail);
        if(!out.submitted) {
            if(out.reference.parameters)s.ReleaseUnsubmitted();
            else out.request_ended=true;
        }
    }
    s.ReadSignals();
    if(out.request_ended)s.Drain();
}
bool OwnedEffectRequest::TrackSubmitted(void* manager,EffectReference reference) {
    auto& s=*state_;auto& out=s.status;
    if(out.started)throw std::logic_error("effect request already started");
    if(!manager || !reference.parameters)throw std::invalid_argument("submitted effect receipt absent");
    // A cancelled owner must still retain a late receipt and cancel it later.
    // No callback/configure/observe_request ownership transfer is performed.
    out.started=true;out.submitted=true;out.reference=reference;s.tracked=true;s.manager=manager;
    s.busy=true;struct Exit{bool& busy;~Exit(){busy=false;}} exit{s.busy};
    try {
        s.Keep(manager);s.Keep(reference.parameters);
        s.receipt_retained=true;
        out.detail="tracking official submission; original callbacks retained";
        s.ObserveEnd();
        if(out.request_ended)s.Drain();
    }catch(const std::exception& e) {
        // Never release or resubmit an input handed off by another producer.
        // Preserve partial roots and the exact receipt for explicit servicing.
        out.detail=std::string("submitted receipt tracking unresolved: ")+e.what();
    }
    return s.receipt_retained;
}
void OwnedEffectRequest::Service() {
    auto& s=*state_;auto& out=s.status;
    if(s.busy || !out.started || out.closed)return;
    s.busy=true;struct Exit{bool& busy;~Exit(){busy=false;}} exit{s.busy};
    if(s.tracked && !s.receipt_retained){s.Drain();return;}
    s.ReadSignals();
    if(!out.request_ended && out.reference.parameters)s.ObserveEnd();
    if(out.request_ended){s.Drain();return;}
    if(!out.cancelled || !out.submitted)return;
    // Retry on a changed explicit cancellation or NEW evidence of a late load,
    // not every frame or because Remove returned false. A completion never adopts the
    // effect into a later action; the same old manager/reference owns removal.
    if(out.removal_attempts && out.loaded_completions==s.removed_at_loaded && s.observed_loads==s.removed_at_observation && s.removal_revision==s.removed_revision)return;
    s.removed_revision=s.removal_revision;
    s.removed_at_loaded=out.loaded_completions;
    s.removed_at_observation=s.observed_loads;
    ++out.removal_attempts;
    s.Trace("remove begin id="+std::to_string(out.reference.id)+" attempt="+std::to_string(out.removal_attempts));
    std::string removal_detail;
    if(s.explicit_removal) {
        out.removal={};
        try {
            if(s.actions.remove) {
                const auto options=s.removal_options;
                const auto result=s.actions.remove(s.manager,out.reference,options);
                out.removal=result.removal;removal_detail=result.detail;
            }else removal_detail="explicit removal provider unavailable";
        }catch(const std::exception& e){removal_detail=e.what();}
    }else out.removal=s.services.access.Remove(s.manager,out.reference,s.fade);
    s.Trace("remove returned="+std::to_string(out.removal.read.returned)+" matched="+std::to_string(out.removal.value)+
        " exception="+std::to_string(out.removal.read.exception));
    out.detail=out.removal.read.returned?(out.removal.value?
        "removal accepted; waiting for official release":"removal unmatched; responsibility remains unresolved"):
        "removal call failed; responsibility remains unresolved";
    if(!removal_detail.empty()){out.detail+="; "+removal_detail;s.Trace("remove detail: "+removal_detail);}
    s.ReadSignals();s.ObserveEnd();
    if(out.request_ended)s.Drain();
}
}
