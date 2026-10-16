#include "AnimationConstructionRoute.hpp"
#include "../../platform/NativeHooks.hpp"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>

namespace hksr::sdk_v1::detail {
namespace {
thread_local const AnimationConstructionArguments* current=nullptr;
std::atomic<std::uint64_t> serial{0};
}
const AnimationConstructionArguments* ExchangeAnimationConstruction(const AnimationConstructionArguments* value) noexcept {
    const auto* previous=current;current=value;return previous;
}
const AnimationConstructionArguments* CurrentAnimationConstruction() noexcept {return current;}
struct AnimationConstructionSubscription::State {
    AnimationConstructionParticipant participant;
    bool projection=false;
    std::atomic<bool> active{true};
    struct Router {
        NativeHooks hook;
        std::mutex install;
        using Clients=std::vector<std::shared_ptr<State>>;
        std::shared_ptr<const Clients> clients=std::make_shared<const Clients>();
        void* entry=nullptr;
    };
    static Router& Route(){static auto* route=new Router;return *route;}
    struct Call {std::shared_ptr<State> state;void* token=nullptr;};
    struct Batch {std::vector<Call> calls;};
    static Batch* Begin(AnimationConstructionArguments& args) noexcept {
        std::unique_ptr<Batch> batch;
        try {
            const auto clients=std::atomic_load(&Route().clients);
            batch=std::make_unique<Batch>();batch->calls.reserve(clients->size());
            for(bool projection:{true,false})for(const auto& client:*clients) {
                if(client->projection!=projection || !client->active.load())continue;
                try {
                    // An observer cannot change arguments for later clients or
                    // the engine, even if its internal implementation tries.
                    auto observed=args;
                    if(void* token=client->participant.begin(projection?args:observed))
                        batch->calls.push_back({client,token});
                }catch(...) {} // participants diagnose their own failures
            }
        }catch(...) {} // nothing has been published before reserve succeeds
        return batch.release();
    }
    static void Finish(Batch* raw,bool returned) noexcept {
        std::unique_ptr<Batch> batch(raw);if(!batch)return;
        // Observe the result while every private projection is still rooted.
        for(auto it=batch->calls.rbegin();it!=batch->calls.rend();++it)
            try{it->state->participant.finish(it->token,returned);}catch(...) {}
    }
    static void __fastcall Detour(void* receiver,void* events,void* behaviors,void* groups,void* logic,
        void* context,void* registration,void* file,void* metadata) {
        const auto input_error=GetLastError();
        AnimationConstructionArguments args{receiver,events,behaviors,groups,logic,context,registration,file,metadata};
        args.serial=++serial;
        const auto* previous=ExchangeAnimationConstruction(nullptr);
        auto* batch=Begin(args);ExchangeAnimationConstruction(&args);SetLastError(input_error);
        using Original=void(__fastcall*)(void*,void*,void*,void*,void*,void*,void*,void*,void*);
        __try {reinterpret_cast<Original>(Route().hook.Original(0))(args.receiver,args.events,args.behaviors,args.groups,
            args.logic,args.context,args.registration,args.file,args.metadata);}
        __finally {const auto output_error=GetLastError();ExchangeAnimationConstruction(nullptr);
            Finish(batch,AbnormalTermination()==FALSE);ExchangeAnimationConstruction(previous);SetLastError(output_error);}
    }
};
AnimationConstructionSubscription::AnimationConstructionSubscription(MethodResolver& methods,bool projection,
    AnimationConstructionParticipant participant) {
    if(!participant.begin || !participant.finish)throw std::invalid_argument("event construction needs paired callbacks");
    const auto method=methods.Resolve("","HHECHLOIEJO","FGAHFIBMPCP","System.Void",{
        "RPG.GameCore.SingleAnimEventConfig[]","RPG.GameCore.SingleAnimBehaviourEvent[]","RPG.GameCore.AnimGroupEventConfig[]",
        "RPG.GameCore.LogicAnimEventEntry[]","RPG.GameCore.TaskContext","LDMKDJJLOIL","System.String"});
    auto& router=State::Route();std::lock_guard<std::mutex> lock(router.install);
    if(router.entry && router.entry!=method.code)throw std::invalid_argument("event construction belongs to another runtime");
    if(!router.entry) {
        if(router.hook.Pending())throw std::logic_error("event construction installation incomplete");
        router.hook.Install({{"animation event construction",method.code,reinterpret_cast<void*>(&State::Detour)}});
        router.entry=method.code;
    }
    auto state=std::make_shared<State>();state->projection=projection;state->participant=std::move(participant);
    auto clients=std::make_shared<State::Router::Clients>(*std::atomic_load(&router.clients));
    clients->push_back(state);state_=std::move(state);
    std::atomic_store(&router.clients,std::shared_ptr<const State::Router::Clients>(std::move(clients)));
}
AnimationConstructionSubscription::~AnimationConstructionSubscription(){Stop();}
void AnimationConstructionSubscription::Stop() noexcept {
    if(!state_)return;state_->active.store(false);
    try {
        auto& router=State::Route();std::lock_guard<std::mutex> lock(router.install);
        auto clients=std::make_shared<State::Router::Clients>(*std::atomic_load(&router.clients));
        clients->erase(std::remove(clients->begin(),clients->end(),state_),clients->end());
        std::atomic_store(&router.clients,std::shared_ptr<const State::Router::Clients>(std::move(clients)));state_.reset();
    }catch(...) {} // inactive subscription retained if snapshot allocation fails
}
}
