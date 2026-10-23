#include <hksr/presentation/movement/AnimationMovementUpdates.hpp>
#include <hksr/native/MetadataReader.hpp>
#include "../../platform/NativeHooks.hpp"
#include <Windows.h>
#include <atomic>
#include <map>
#include <mutex>
#include <stdexcept>

namespace hksr::sdk_v1 {
namespace {
template<class T>bool Read(void* object,std::size_t offset,T& value) noexcept {
    return object && native::CopyMemoryChecked(&value,static_cast<char*>(object)+offset,sizeof(value));
}
}
struct AnimationMovementUpdates::State {
    struct Router {
        detail::NativeHooks hooks;std::mutex install,mutex;
        std::map<void*,State*> watched;void* target=nullptr;
        std::atomic<unsigned> count{0};
    };
    static Router& Route(){static auto* r=new Router;return *r;}
    AnimationMovementAccessBinding binding;GameEntity* owner;void* component;void* graph;
    DWORD thread=GetCurrentThreadId();std::atomic<unsigned> references{1};
    AnimationMovementUpdateStatus status;unsigned depth=0;
    State(AnimationMovementAccessBinding b,GameEntity* o,void* c,void* g):binding(b),owner(o),component(c),graph(g){}
    static void Release(State* s) noexcept {if(s && --s->references==0)delete s;}
    static State* Acquire(void* component) noexcept {
        auto& r=Route();if(!r.count.load(std::memory_order_relaxed))return nullptr;
        try {
            std::lock_guard<std::mutex> lock(r.mutex);
            const auto found=r.watched.find(component);
            if(found==r.watched.end() || found->second->thread!=GetCurrentThreadId())return nullptr;
            auto* s=found->second;++s->references;return s;
        }catch(...){return nullptr;}
    }
    bool Before() noexcept {
        ++depth;
        if(depth!=1){status.failed=true;return false;}
        if(status.completed || status.interrupted || status.replaced || status.failed)return false;
        void* actual=nullptr;void* entity=nullptr;
        if(!Read(component,binding.data,actual) || !Read(component,binding.owner,entity)) {status.failed=true;return false;}
        // A watch is installed before Submit, which can first evaluate an empty
        // channel. Those calls, and other graphs, are not this request's updates.
        if(actual!=graph)return false;
        if(entity!=owner){status.failed=true;return false;}
        return true;
    }
    void After(bool captured,bool result,bool abnormal) noexcept {
        if(captured && !status.failed) {
            if(abnormal)status.failed=true;
            else {
                void* actual=nullptr;void* entity=nullptr;bool active=false;
                float animation=0,movement=0;
                if(!Read(component,binding.data,actual) || !Read(component,binding.owner,entity) || entity!=owner ||
                    !Read(component,binding.active,active) || !Read(graph,binding.graph.previous_time,animation) ||
                    !Read(graph,binding.graph.progress,movement))status.failed=true;
                else {
                    status.observed=true;++status.updates;
                    status.animation_time=animation;status.movement_time=movement;
                    status.started=status.started || result || (actual==graph && active);
                    // In the reviewed evaluator, normal true + cleared FGNN is
                    // the completed segment/end-cue path. A changed animation
                    // clears it and returns false. A detached graph observed
                    // outside this boundary cannot establish completion.
                    if(!actual){status.completed=result;status.interrupted=!result;}
                    else if(actual!=graph)status.replaced=true;
                }
            }
        }
        --depth;
    }
    using UpdateFn=bool(__fastcall*)(void*,float,Position3*,void*);
    static bool __fastcall Update(void* self,float delta,Position3* position,void* method) {
        auto* s=Acquire(self);const bool captured=s && s->Before();bool result=false;
        // No C++ object requiring unwinding here. Every acquired observation
        // is released even when the original native/managed exception escapes.
        __try {result=reinterpret_cast<UpdateFn>(Route().hooks.Original(0))(self,delta,position,method);}
        __finally {if(s)s->After(captured,result,AbnormalTermination()!=FALSE);Release(s);}
        return result;
    }
};
AnimationMovementUpdates::AnimationMovementUpdates(Method update,AnimationMovementAccessBinding binding,
    GameEntity* owner,void* component,void* graph) {
    if(!update.code || !owner || !component || !graph)throw std::invalid_argument("movement observation inputs incomplete");
    auto& r=State::Route();
    {
        std::lock_guard<std::mutex> lock(r.install);
        if(r.target && r.target!=update.code)throw std::logic_error("movement evaluator binding changed in this process");
        if(!r.target) {
            r.hooks.Install({{"animation movement evaluation",update.code,reinterpret_cast<void*>(&State::Update)}});
            r.target=update.code;
        }
    }
    auto s=std::make_unique<State>(binding,owner,component,graph);
    std::lock_guard<std::mutex> lock(r.mutex);
    if(!r.watched.emplace(component,s.get()).second)throw std::logic_error("movement component already observed by another request");
    ++r.count;state_=s.release();
}
AnimationMovementUpdates::~AnimationMovementUpdates(){Close();}
AnimationMovementUpdateStatus AnimationMovementUpdates::Status()const {
    if(!state_)throw std::logic_error("movement observer closed");
    if(state_->thread!=GetCurrentThreadId())throw std::logic_error("movement observation must be read on its engine thread");
    return state_->status;
}
void AnimationMovementUpdates::Close() noexcept {
    if(!state_)return;
    auto& r=State::Route();
    try {
        std::lock_guard<std::mutex> lock(r.mutex);
        const auto found=r.watched.find(state_->component);
        if(found!=r.watched.end() && found->second==state_){r.watched.erase(found);--r.count;}
        State::Release(state_);state_=nullptr;
    }catch(...){} // retain the registration on lock failure
}
}
