#include <hksr/native/ActorInputPolicy.hpp>
#include <Windows.h>
#include <MinHook.h>
#include <atomic>
#include <algorithm>
#include <mutex>
#include <vector>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct ActorInputPolicy::State {
    ActorInputKind kind;
    Handler handler;
    std::atomic<DWORD> thread{0};std::atomic<bool> stopped{false};
    struct Router {
        std::mutex mutex;void* target=nullptr;void* original=nullptr;bool enabled=false;
        using Clients=std::vector<std::shared_ptr<State>>;
        std::shared_ptr<const Clients> clients=std::make_shared<const Clients>();
    };
    static Router& Route(ActorInputKind kind) {
        static auto* outfit=new Router;static auto* weapon=new Router;
        return kind==ActorInputKind::OutfitIdentity?*outfit:*weapon;
    }
    template<ActorInputKind Kind> static void __fastcall Dispatch(void* receiver,void* input,void* metadata) {
        const auto error=GetLastError();auto& router=Route(Kind);
        const auto clients=std::atomic_load(&router.clients);
        for(const auto& client:*clients) {
            if(client->stopped.load() || client->thread.load()!=GetCurrentThreadId())continue;
            if(const auto replacement=client->handler(receiver,input)){input=*replacement;break;}
        }
        SetLastError(error);
        reinterpret_cast<void(__fastcall*)(void*,void*,void*)>(router.original)(receiver,input,metadata);
    }
};
ActorInputPolicy::ActorInputPolicy(ActorInputKind kind,Method method,Handler handler):state_(std::make_shared<State>()) {
    if(!method.code || !handler)throw std::invalid_argument("actor input policy needs a target and handler");
    state_->kind=kind;state_->handler=std::move(handler);
    auto& r=State::Route(kind);std::lock_guard<std::mutex> lock(r.mutex);
    if(r.target && r.target!=method.code)throw std::runtime_error("actor input target changed within this process");
    if(!r.target) {
        auto result=MH_Initialize();if(result!=MH_OK && result!=MH_ERROR_ALREADY_INITIALIZED)throw std::runtime_error(MH_StatusToString(result));
        auto* detour=kind==ActorInputKind::OutfitIdentity?reinterpret_cast<void*>(State::Dispatch<ActorInputKind::OutfitIdentity>):reinterpret_cast<void*>(State::Dispatch<ActorInputKind::WeaponVisibility>);
        result=MH_CreateHook(method.code,detour,&r.original);if(result!=MH_OK)throw std::runtime_error(MH_StatusToString(result));
        r.target=method.code;
    }
    if(!r.enabled) {
        const auto result=MH_EnableHook(r.target);if(result!=MH_OK && result!=MH_ERROR_ENABLED)throw std::runtime_error(MH_StatusToString(result));r.enabled=true;
    }
    auto next=std::make_shared<State::Router::Clients>(*std::atomic_load(&r.clients));next->push_back(state_);
    std::atomic_store(&r.clients,std::shared_ptr<const State::Router::Clients>(std::move(next)));
}
ActorInputPolicy::~ActorInputPolicy(){Stop();}
void ActorInputPolicy::ActivateOnCurrentThread(){state_->thread.store(GetCurrentThreadId());}
void ActorInputPolicy::Stop() noexcept {
    if(!state_ || state_->stopped.exchange(true))return;
    try {
        auto& r=State::Route(state_->kind);std::lock_guard<std::mutex> lock(r.mutex);
        auto next=std::make_shared<State::Router::Clients>(*std::atomic_load(&r.clients));
        next->erase(std::remove(next->begin(),next->end(),state_),next->end());
        std::atomic_store(&r.clients,std::shared_ptr<const State::Router::Clients>(std::move(next)));
    }catch(...) {} // Deactivated even if compaction of the client list fails.
}
}
