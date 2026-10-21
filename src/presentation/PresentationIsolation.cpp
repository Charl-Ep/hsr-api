#include <hksr/presentation/PresentationIsolation.hpp>
#include "../runtime/RuntimeResident.hpp"
#include "timeline/TimelineWaits.hpp"
#include "timeline/TimelineEnvironment.hpp"
#include "timeline/CommonTimelinePlayback.hpp"
#include "effects/EffectScopes.hpp"
#include "PresentationClients.hpp"
#include "../runtime/RepeatedDiagnostics.hpp"
#include <Windows.h>
#include <mutex>
#include <atomic>
#include <sstream>

namespace hksr::sdk_v1 {
namespace {
std::optional<detail::TimelineOwner> Owner(EntityOrigin origin) {
    if(!origin.token.entity)return {};
    return detail::TimelineOwner{reinterpret_cast<std::uintptr_t>(origin.world),
        reinterpret_cast<std::uintptr_t>(origin.token.entity),origin.token.generation};
}
EntityOrigin Origin(detail::TimelineOwner owner) {
    return {{reinterpret_cast<GameEntity*>(owner.entity),owner.lifetime},reinterpret_cast<GameWorld*>(owner.world)};
}
}
struct PresentationIsolation::State {
    struct Shared;
    std::shared_ptr<Shared> service;
    std::uint64_t client=0;
    std::map<void*,std::uint32_t> requests;
    State(RuntimeMetadata&,ReferenceMethods,PresentationIsolationPolicy,Trace);
    ~State();
};
struct PresentationIsolation::State::Shared {
    // First member, destroyed last. No Framework dependency survives retirement.
    std::shared_ptr<RuntimeMetadata> metadata;
    PresentationSources sources;
    detail::PresentationClients clients;
    std::recursive_mutex trace_mutex;
    detail::RepeatedDiagnostics diagnostics;
    std::atomic<unsigned long> thread{0};
    bool collected=false;
    std::uint64_t tail_errors=0;
    std::string last_tail_error;
    std::map<std::string,std::string> collection_errors;
    std::unique_ptr<detail::TimelineRegistryTracking> registry;
    std::unique_ptr<detail::TimelineWaits> waits;
    std::unique_ptr<detail::TimelineTasks> tasks;
    std::unique_ptr<detail::EffectQueries> queries;
    std::unique_ptr<detail::EffectScopes> effects;
    std::unique_ptr<detail::TimelineEnvironment> environment;
    std::unique_ptr<detail::CommonTimelinePlayback> playback;
    ReferenceMethods references;
    static auto& Slot(){static auto* value=new detail::PresentationServiceSlot<Shared>;return *value;}
    void Log(const std::string& text) noexcept {
        try {std::lock_guard<std::recursive_mutex> lock(trace_mutex);
            if(!diagnostics.Publish(text))return;
            ++tail_errors;last_tail_error=text;clients.Log(text);}catch(...){}
    }
    Shared(RuntimeMetadata& m,ReferenceMethods refs)
        :metadata(detail::RetainRuntimeMetadata(m)),sources(refs),references(refs) {
        if(m.Version()!=RuntimeVersion::Hsr460)throw std::invalid_argument("presentation isolation requires the qualified 4.6 provider");
        const auto fields=[this](const char* ns,const char* type){return metadata->ReadFields(ns,type);};
        const auto log=[this](const std::string& text){Log(text);};
        registry=std::make_unique<detail::TimelineRegistryTracking>(m,m,m,refs,fields,log);
        waits=std::make_unique<detail::TimelineWaits>(m,m,m,refs,fields,*registry,log);
        tasks=std::make_unique<detail::TimelineTasks>(m,refs,fields,
            [this](GameEntity* entity){return sources.Identify(entity);},
            [this](const auto& call){return waits->Before(call);},
            [this](void* receipt,const auto& call,bool abnormal){waits->After(receipt,call,abnormal);},log);
        const detail::EffectQueries::Source effect_source=[this](auto ref,const auto& facts){return Owner(sources.Identify(ref,facts));};
        queries=std::make_unique<detail::EffectQueries>(m,m,m,refs,fields,effect_source,log);
        effects=std::make_unique<detail::EffectScopes>(m,refs,fields,effect_source,log,
            [this](auto owner){return clients.Policy(Origin(owner)).allow_main_camera;},
            std::make_shared<detail::EffectViewPolicy>(detail::MakeEffectViewClassifier(m,m,refs,fields,
                [&m](void* list){return ResolveReferenceListHierarchy(m,m.ObjectType(list),"RPG.Client.MonoEffectPluginBase");}),
                [this](auto owner){return clients.CameraPresentations(Origin(owner));},
                [this](auto ref,auto owner,bool visible){Log("camera-presentation effect="+std::to_string(ref.id)+
                    " entity="+std::to_string(owner.entity)+" generation="+std::to_string(owner.lifetime)+" visible="+std::to_string(visible));}));
        environment=std::make_unique<detail::TimelineEnvironment>(m,m,refs,fields,
            [this](void* director){return registry->ObserveDirector(director);},
            [this](auto owner,auto){return clients.Policy(Origin(owner)).allow_timeline_environment;},log);
    }
    void CheckThread() const {
        if(!thread || thread!=GetCurrentThreadId())throw std::logic_error("presentation operation requires its activated engine thread");
    }
    detail::CommonTimelinePlayback& Playback() {
        CheckThread();
        if(!playback)playback=std::make_unique<detail::CommonTimelinePlayback>(*metadata,*metadata,*metadata,references,*registry);
        return *playback;
    }
    void Activate() {
        if(thread && thread!=GetCurrentThreadId())throw std::logic_error("presentation isolation cannot migrate engine threads");
        registry->ActivateOnCurrentThread();tasks->ActivateOnCurrentThread();
        queries->ActivateOnCurrentThread();effects->ActivateOnCurrentThread();environment->ActivateOnCurrentThread();
        thread=GetCurrentThreadId();
    }
    void Collect() {
        CheckThread();
        if(collected)return;
        collected=true; // one shared collection per official delivery, not per client
        // Independent failures do not suppress other consumers' collection.
        const auto run=[this](const char* name,auto f){
            try{f();collection_errors.erase(name);}
            catch(const std::exception& e){
                auto& previous=collection_errors[name];
                if(previous!=e.what()){previous=e.what();Log(std::string(name)+": "+previous);}
            }
        };
        run("source collection",[&]{sources.Collect();});
        run("task collection",[&]{tasks->Collect();});
        run("wait collection",[&]{waits->Collect();});
        run("environment collection",[&]{environment->Collect();});
        run("history collection",[&]{registry->CollectHistories([&](auto owner){return tasks->HasSource(owner) || sources.HasSource(Origin(owner));});});
        run("client policy collection",[&]{clients.Collect([&](EntityOrigin origin){
            const auto owner=Owner(origin);
            return sources.HasSource(origin) || (owner && (tasks->HasSource(*owner) || registry->Ownership().HasPending(*owner)));
        });});
    }
    static void ServiceTails() noexcept {
        auto s=Slot().Existing();
        if(s && s->thread==GetCurrentThreadId()) {
            s->collected=false;
            try{s->Collect();}catch(...){s->Log("retired presentation collection failed");}
        }
    }
};
PresentationIsolation::State::State(RuntimeMetadata& m,ReferenceMethods refs,PresentationIsolationPolicy policy,Trace trace) {
    if(m.Version()!=RuntimeVersion::Hsr460)throw std::invalid_argument("presentation isolation requires the qualified 4.6 provider");
    service=Shared::Slot().Acquire([&]{return std::make_shared<Shared>(m,refs);});
    if(service->metadata.get()!=&m)throw std::invalid_argument("presentation service belongs to another metadata provider");
    client=service->clients.Add(policy,std::move(trace));
}
PresentationIsolation::State::~State(){service->clients.Detach(client);}
PresentationIsolation::PresentationIsolation(RuntimeMetadata& m,ReferenceMethods refs,PresentationIsolationPolicy p,Trace trace)
    :state_(std::make_unique<State>(m,refs,p,std::move(trace))){}
PresentationIsolation::~PresentationIsolation()=default;
void PresentationIsolation::ActivateOnCurrentThread(){if(state_)state_->service->Activate();}
void PresentationIsolation::SetCameraPresentations(bool enabled) {
    if(!state_)throw std::logic_error("presentation client retired");
    state_->service->CheckThread();state_->service->clients.SetCameraPresentations(state_->client,enabled);
}
void PresentationIsolation::RememberBirth(EntityOrigin origin){
    if(!state_)throw std::logic_error("presentation client retired");
    auto& service=*state_->service;service.CheckThread();
    try {
        service.clients.Remember(state_->client,origin);
        service.sources.Remember(origin);
        std::ostringstream text;text<<"presentation birth world="<<origin.world<<" entity="<<origin.token.entity<<" lifetime="<<origin.token.generation;
        service.Log(text.str());
    }catch(const std::exception& e){service.Log(std::string("birth attribution unresolved; creation continues: ")+e.what());}
}
void PresentationIsolation::RememberRequest(EffectReference ref,GameEntity* owner){
    if(!state_)throw std::logic_error("presentation client retired");
    auto& service=*state_->service;service.CheckThread();
    try{service.clients.RequireOwner(state_->client,service.sources.Identify(owner));state_->requests[ref.parameters]=ref.id;service.sources.RememberRequest(ref,owner);}
    catch(const std::exception& e){service.Log(std::string("explicit effect attribution unresolved; submission continues: ")+e.what());}
}
EntityOrigin PresentationIsolation::Source(GameEntity* entity) const {
    if(!state_ || !entity)return {};
    auto& service=*state_->service;service.CheckThread();
    const auto origin=service.sources.Identify(entity);
    return service.clients.Owns(state_->client,origin)?origin:EntityOrigin{};
}
void PresentationIsolation::RetireRequest(EffectReference ref){
    if(!state_)throw std::logic_error("presentation client retired");state_->service->CheckThread();
    const auto i=state_->requests.find(ref.parameters);
    if(i==state_->requests.end() || i->second!=ref.id)return;
    state_->service->sources.RetireRequest(ref);state_->requests.erase(i);
}
void PresentationIsolation::Collect(){if(state_)state_->service->Collect();}
std::unique_ptr<OwnedTimeline> PresentationIsolation::CreateTimeline(EntityOrigin origin) {
    if(!state_)throw std::logic_error("presentation client retired");
    const auto shared=state_->service;const auto client=state_->client;
    shared->CheckThread();shared->clients.RequireOwner(client,origin);
    const auto owner=Owner(origin).value();
    return std::make_unique<OwnedTimeline>(TimelineRequestServices{
        [shared,client,origin](const std::string& path,int wrap,double time) {
            shared->CheckThread();shared->clients.RequireOwner(client,origin);
            return shared->Playback().Start(origin,path,wrap,time);
        },
        [shared,owner](TimelineReference ref){return shared->Playback().Observe(owner,ref);},
        [shared,owner](TimelineReference ref){return shared->Playback().Stop(owner,ref);}});
}
void PresentationIsolation::Retire() {
    if(!state_)return;state_->service->CheckThread();
    state_->service->Log("presentation isolation: client retired; shared SDK service retains official playback tails");
    state_.reset();
}
namespace detail {
void ServiceRetiredPresentations() noexcept {PresentationIsolation::State::Shared::ServiceTails();}
}
}
