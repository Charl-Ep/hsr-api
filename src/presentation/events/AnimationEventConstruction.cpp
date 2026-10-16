#include <hksr/presentation/events/AnimationEventConstruction.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include "AnimationConstructionRoute.hpp"
#include "../../platform/NativeHooks.hpp"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>

namespace hksr::sdk_v1 {
struct AnimationEventConstruction::State {
    struct Router {
        detail::NativeHooks hook;void* entry=nullptr;std::mutex install;
        using Clients=std::vector<std::shared_ptr<State>>;
        std::shared_ptr<const Clients> clients=std::make_shared<const Clients>();
    };
    static Router& Route(){static auto* route=new Router;return *route;}
    Method caster;ReferenceMethods references;
    Identify identify;Before before;Tasks tasks;After after;Trace trace;
    std::atomic<DWORD> thread{0};
    struct Pins {
        ManagedRoots roots;std::vector<ManagedRootId> ids;
        explicit Pins(ReferenceMethods refs):roots(refs){}
        void Keep(void* p) {
            if(!p)return;ids.reserve(ids.size()+1);
            const auto id=roots.Keep(p);ids.push_back(id);
            if(!roots.Status(id).ready)throw std::runtime_error("event source retention failed");
        }
        bool Close() {
            bool closed=true;
            for(auto id:ids){if(roots.Release(id).complete)roots.Forget(id);else closed=false;}
            return closed;
        }
    };
    struct Call {
        std::shared_ptr<State> state;
        AnimationEventSource source;
        std::unique_ptr<Pins> pins;
    };
    std::map<std::uint64_t,Call*> calls; // owner-thread scoped; no permanent game roots
    State(MethodResolver& methods,ReferenceMethods refs,Identify who,Before b,Tasks t,After a,Trace log)
        :caster(methods.Resolve("RPG.GameCore","TaskContext","get_CasterEntity","RPG.GameCore.GameEntity",{})),
        references(refs),identify(std::move(who)),before(std::move(b)),tasks(std::move(t)),after(std::move(a)),trace(std::move(log)){}
    void Log(const std::string& message) noexcept {try{if(trace)trace("event construction observation: "+message);}catch(...) {}}
    void Close(std::unique_ptr<Pins>& pins) noexcept {
        if(!pins)return;
        try{if(pins->Close()){pins.reset();return;}}catch(...) {}
        pins.release();Log("GC cleanup unresolved; receipt retained");
    }
    static void* Begin(const std::shared_ptr<State>& s,const detail::AnimationConstructionArguments& args) noexcept {
        if(s->thread.load()!=GetCurrentThreadId() || !args.context)return nullptr;
        std::unique_ptr<Call> call;
        try {
            call=std::make_unique<Call>();call->state=s;call->pins=std::make_unique<Pins>(s->references);
            call->pins->Keep(args.context);
            GameEntity* caster=nullptr;
            if(!native::Call(s->caster,&caster,args.context).returned)throw std::runtime_error("event caster lookup failed");
            call->pins->Keep(caster);
            const auto origin=caster?s->identify(caster):EntityOrigin{};
            if(!origin.token.entity){s->Close(call->pins);return nullptr;}
            if(origin.token.entity!=caster || !origin.token.generation || !origin.world)
                throw std::runtime_error("event source generation disagrees");
            for(auto* p:{args.receiver,args.events,args.behaviors,args.groups,args.logic,args.registration,args.file})call->pins->Keep(p);
            call->source={args.serial,origin,args.receiver,args.events,args.behaviors,args.groups,args.logic,args.context,args.registration,args.file};
            s->calls.emplace(args.serial,call.get());
            try{s->before(call->source);}catch(...){s->Log("before observer failed; original forwarded");}
            return call.release();
        }catch(const std::exception& e){s->Log(e.what());}catch(...){s->Log("source observation failed");}
        if(call)s->Close(call->pins);return nullptr;
    }
    static void Finish(void* raw,bool returned) noexcept {
        std::unique_ptr<Call> call(static_cast<Call*>(raw));if(!call)return;
        auto& s=*call->state;
        if(s.thread.load()==GetCurrentThreadId())try{s.after(call->source,returned);}catch(...){s.Log("after observer failed");}
        s.calls.erase(call->source.serial);s.Close(call->pins);
    }
    static void Built(const detail::AnimationConstructionArguments* construction,const AnimationTaskConstruction& value) noexcept {
        if(!construction)return;
        const auto clients=std::atomic_load(&Route().clients);
        for(const auto& s:*clients) {
            if(s->thread.load()!=GetCurrentThreadId())continue;
            const auto found=s->calls.find(construction->serial);
            if(found==s->calls.end())continue;
            auto* call=found->second;
            // Preserve actual nested inputs even when their context differs.
            // Association with this construction call is not child ownership.
            std::unique_ptr<Pins> pins;
            try {
                pins=std::make_unique<Pins>(s->references);
                for(auto* p:{value.context,value.configurations,value.parent,value.tasks})pins->Keep(p);
                s->tasks(call->source,value);
            }catch(const std::exception& e){s->Log(e.what());}catch(...){s->Log("task construction observer failed");}
            s->Close(pins);
        }
    }
    static void __fastcall Notify(const detail::AnimationConstructionArguments* construction,const AnimationTaskConstruction& value) noexcept {
        const auto* previous=detail::ExchangeAnimationConstruction(nullptr);
        Built(construction,value);detail::ExchangeAnimationConstruction(previous);
    }
    static void* __fastcall Build(void* context,void* configs,void* parent,void* metadata) {
        static std::atomic<std::uint64_t> next{0};
        const auto* construction=detail::CurrentAnimationConstruction();
        using Original=void*(__fastcall*)(void*,void*,void*,void*);
        if(!construction)return reinterpret_cast<Original>(Route().hook.Original(0))(context,configs,parent,metadata);
        AnimationTaskConstruction value{++next,context,configs,parent,nullptr,false};
        __try {value.tasks=reinterpret_cast<Original>(Route().hook.Original(0))(context,configs,parent,metadata);value.original_returned=true;}
        __finally {const auto error=GetLastError();Notify(construction,value);SetLastError(error);}
        return value.tasks;
    }
};
struct AnimationEventConstruction::Holder {
    std::shared_ptr<State> state;
    std::unique_ptr<detail::AnimationConstructionSubscription> subscription;
};
AnimationEventConstruction::AnimationEventConstruction(MethodResolver& methods,ReferenceMethods refs,Identify identify,
    Before before,Tasks tasks,After after,Trace trace) {
    if(!identify || !before || !tasks || !after)throw std::invalid_argument("event construction requires source and callbacks");
    auto state=std::make_shared<State>(methods,refs,std::move(identify),std::move(before),std::move(tasks),std::move(after),std::move(trace));
    const auto builder=methods.Resolve("","JKNLHJLPIOO","KJCNEJALIJO","GELIALJDFBL[]",{
        "RPG.GameCore.TaskContext","System.Collections.Generic.IEnumerable<RPG.GameCore.TaskConfig>","BNBOAMEBAOE"},MethodKind::Static);
    auto holder=std::make_unique<Holder>();holder->state=state;
    auto& router=State::Route();std::lock_guard<std::mutex> lock(router.install);
    if(router.entry && router.entry!=builder.code)throw std::invalid_argument("event task builder belongs to another runtime");
    if(!router.entry) {
        if(router.hook.Pending())throw std::logic_error("event task observation installation incomplete");
        router.hook.Install({{"event task construction",builder.code,reinterpret_cast<void*>(&State::Build)}});router.entry=builder.code;
    }
    holder->subscription=std::make_unique<detail::AnimationConstructionSubscription>(methods,false,
        detail::AnimationConstructionParticipant{[state](auto& args){return State::Begin(state,args);},State::Finish});
    auto clients=std::make_shared<State::Router::Clients>(*std::atomic_load(&router.clients));clients->push_back(state);
    std::atomic_store(&router.clients,std::shared_ptr<const State::Router::Clients>(std::move(clients)));
    holder_=holder.release();
}
AnimationEventConstruction::~AnimationEventConstruction(){Stop();}
void AnimationEventConstruction::ActivateOnCurrentThread() {
    if(!holder_)return;
    DWORD empty=0;const auto current=GetCurrentThreadId();
    if(!holder_->state->thread.compare_exchange_strong(empty,current) && empty!=current)
        throw std::logic_error("event construction observer cannot migrate engine threads");
}
void AnimationEventConstruction::Stop() noexcept {
    if(!holder_)return;holder_->state->thread.store(0);holder_->subscription->Stop();
    try {
        auto& router=State::Route();std::lock_guard<std::mutex> lock(router.install);
        auto clients=std::make_shared<State::Router::Clients>(*std::atomic_load(&router.clients));
        clients->erase(std::remove(clients->begin(),clients->end(),holder_->state),clients->end());
        std::atomic_store(&router.clients,std::shared_ptr<const State::Router::Clients>(std::move(clients)));
        delete holder_;holder_=nullptr;
    }catch(...) {}
}
}
