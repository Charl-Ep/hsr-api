#include <hksr/native/ActorEmotion.hpp>
#include <hksr/runtime/OperationTrace.hpp>
#include <hksr/native/Invocation.hpp>
#include <Windows.h>
#include <MinHook.h>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace hksr::sdk_v1 {
namespace {
void Require(CallResult call,const char* step) {
    if(!call.returned)throw std::runtime_error(std::string(step)+" exception="+std::to_string(call.exception));
}
}
ActorEmotionMethods ResolveActorEmotionMethods(MethodResolver& m) {
    ActorEmotionMethods out;
    out.graph=m.Resolve("","PEPNFLNCDHG","AGEEOFENFMP","RPG.Client.MonoEmoPlayableGraph",{});
    out.config_path=m.Resolve("RPG.GameCore","CharacterDataComponent","get_GraphEmotionAsset","System.String",{});
    out.apply_config=m.Resolve("","PEPNFLNCDHG","BDIIADDGLBO","System.Void",{"RPG.Client.EmoPlayableGraphConfig"});
    out.select_mode=m.Resolve("","PEPNFLNCDHG","GNKBMOIOFFC","System.Void",{"System.Boolean"});
    out.auto_blink=m.Resolve("RPG.Client","MonoEmoPlayableGraph","SetEnableAutoBlinkState","System.Void",{"System.Boolean"});
    out.eye_blink=m.Resolve("RPG.Client","MonoEmoPlayableGraph","SetEnableAutoBlickWithEyeState","System.Void",{"System.Boolean"});
    out.always_evaluate=m.Resolve("RPG.Client","MonoEmoPlayableGraph","set_AlwaysEvaluate","System.Void",{"System.Boolean"});
    out.built_graph=m.Resolve("RPG.Client","MonoEmoPlayableGraph","get_emotionPlayableGraph","BKLJMPGNFNB",{});
    return out;
}
ActorEmotionSetup::ActorEmotionSetup(ActorEmotionMethods methods,ReferenceMethods references)
    :methods_(methods),assets_(references){}
void ActorEmotionSetup::Apply(void* emotion,void* data,void* config_type,bool graph_mode) {
    OperationSpan timing("sdk.emotion.initialize");
    if(status_.attempted)return; // One construction, no automatic native retries.
    status_.attempted=true;status_.requested_graph=graph_mode;
    try {
        Require(native::Call(methods_.graph,&graph_,emotion),"read emotion graph");
        status_.has_graph=graph_!=nullptr;
        if(!graph_){status_.ready=true;return;} // Official wrapper also returns here.
        if(graph_mode) {
            void* path=nullptr;
            Require(native::Call(methods_.config_path,&path,data),"read graph configuration path");
            if(!path || !config_type)throw std::runtime_error("graph mode requires its official configuration");
            config_=assets_.Acquire(path,config_type,false);
            if(!assets_.Status(config_).ready)throw std::runtime_error("emotion configuration acquisition failed");
            status_.config_loaded=true;
            Require(native::CallVoid(methods_.apply_config,emotion,assets_.Object(config_)),"apply emotion configuration");
            Require(native::CallVoid(methods_.select_mode,emotion,true),"enable configured emotion graph");
            // Unity can report OnEnable errors without making set_enabled fail.
            void* built=nullptr;
            Require(native::Call(methods_.built_graph,&built,graph_),"read initialized emotion graph");
            if(!built)throw std::runtime_error("emotion graph initialization did not complete");
            status_.graph_built=true;
        }
        Require(native::CallVoid(methods_.auto_blink,graph_,true),"set automatic blink");
        Require(native::CallVoid(methods_.eye_blink,graph_,true),"set blink eye state");
        Require(native::CallVoid(methods_.always_evaluate,graph_,graph_mode),"set graph evaluation mode");
        if(!graph_mode)Require(native::CallVoid(methods_.select_mode,emotion,false),"select animator emotion");
        status_.ready=true;
    }catch(const std::exception& e) {
        status_.error=e.what();
        // Never leave an owned failed graph registered in the global evaluator.
        // This is rollback, not a successful fallback to a different mode.
        if(graph_)status_.rolled_back=native::CallVoid(methods_.select_mode,emotion,false).returned;
    }
}
bool ActorEmotionSetup::Close() {
    if(!config_)return true;
    if(!assets_.Release(config_).complete)return false;
    assets_.Forget(config_);config_=0;return true;
}

struct ActorEmotionBoundary::State {
    Handler handler;
    std::atomic<DWORD> thread{0};
    std::atomic<bool> stopped{false};
    struct Router {
        std::mutex mutex;
        void* target=nullptr;
        void* original=nullptr;
        bool enabled=false;
        using Clients=std::vector<std::shared_ptr<State>>;
        std::shared_ptr<const Clients> clients=std::make_shared<const Clients>();
    };
    static Router& Route(){static auto* r=new Router;return *r;}
    static void __fastcall Dispatch(void* component,void* metadata) {
        const auto incoming=GetLastError();auto& r=Route();
        const auto clients=std::atomic_load(&r.clients);
        for(const auto& client:*clients) {
            if(client->stopped.load() || client->thread.load()!=GetCurrentThreadId())continue;
            // Handler is responsible for reporting/rolling back owned failures.
            // Exceptions are not swallowed and followed by unsafe original setup.
            if(client->handler(component)){SetLastError(incoming);return;}
        }
        SetLastError(incoming);
        reinterpret_cast<void(__fastcall*)(void*,void*)>(r.original)(component,metadata);
    }
};
ActorEmotionBoundary::ActorEmotionBoundary(Method method,Handler handler)
    :state_(std::make_shared<State>()) {
    if(!method.code || !handler)throw std::invalid_argument("emotion boundary needs a method and owner handler");
    state_->handler=std::move(handler);auto& r=State::Route();std::lock_guard<std::mutex> lock(r.mutex);
    if(r.target && r.target!=method.code)throw std::runtime_error("emotion boundary already targets another image");
    if(!r.target) {
        auto result=MH_Initialize();
        if(result!=MH_OK && result!=MH_ERROR_ALREADY_INITIALIZED)throw std::runtime_error(MH_StatusToString(result));
        result=MH_CreateHook(method.code,reinterpret_cast<void*>(State::Dispatch),&r.original);
        if(result!=MH_OK)throw std::runtime_error(MH_StatusToString(result));
        r.target=method.code; // Resident even if enabling fails; no dangling trampoline.
    }
    if(!r.enabled) {
        const auto result=MH_EnableHook(r.target);
        if(result!=MH_OK && result!=MH_ERROR_ENABLED)throw std::runtime_error(MH_StatusToString(result));
        r.enabled=true;
    }
    auto next=std::make_shared<State::Router::Clients>(*std::atomic_load(&r.clients));next->push_back(state_);
    std::atomic_store(&r.clients,std::shared_ptr<const State::Router::Clients>(std::move(next)));
}
ActorEmotionBoundary::~ActorEmotionBoundary(){Stop();}
void ActorEmotionBoundary::ActivateOnCurrentThread(){state_->thread.store(GetCurrentThreadId());}
void ActorEmotionBoundary::Stop() noexcept {
    if(!state_ || state_->stopped.exchange(true))return;
    try {
        auto& r=State::Route();std::lock_guard<std::mutex> lock(r.mutex);
        auto next=std::make_shared<State::Router::Clients>(*std::atomic_load(&r.clients));
        next->erase(std::remove(next->begin(),next->end(),state_),next->end());
        std::atomic_store(&r.clients,std::shared_ptr<const State::Router::Clients>(std::move(next)));
    }catch(...) {} // The stopped client remains inert if list allocation fails.
}
}
