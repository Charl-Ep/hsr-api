#include "TimelineRegistryHooks.hpp"
#include "../../../adapters/presentation/TimelineBoundaryMethods.hpp"
#include "../../platform/NativeHooks.hpp"
#include <Windows.h>
#include <atomic>
#include <mutex>
#include <stdexcept>

namespace hksr::sdk_v1::detail {
struct TimelineRegistryHooks::State {
    struct Router {NativeHooks hooks;Method stop_record;std::mutex install;std::shared_ptr<State> current;bool installed=false;};
    static Router& Route(){static auto* router=new Router;return *router;}
    Begin begin;End end;Trace trace;std::atomic<DWORD> thread{0};
    struct Invocation {std::shared_ptr<State> state;void* receipt=nullptr;TimelineBoundaryCall call;};
    State(Begin b,End e,Trace t):begin(std::move(b)),end(std::move(e)),trace(std::move(t)){}
    void Log(const std::string& phase) noexcept {try{if(trace)trace("Timeline boundary callback failed: "+phase);}catch(...) {}}
    static Invocation* Enter(TimelineBoundaryCall& call) noexcept {
        const auto state=std::atomic_load(&Route().current);
        if(!state || state->thread.load()!=GetCurrentThreadId())return nullptr;
        std::unique_ptr<Invocation> invocation;
        try {
            invocation=std::make_unique<Invocation>();invocation->state=state;invocation->call=call;
            invocation->receipt=state->begin(invocation->call);
            // Publish only the supported argument. All remaining physical
            // arguments must reach the original entry completely unchanged.
            if(call.operation==TimelineBoundary::Borrow || call.operation==TimelineBoundary::StopName)
                call.argument=invocation->call.argument;
            if(call.operation==TimelineBoundary::ResourceLookup) {
                call.handled=invocation->call.handled;call.result=invocation->call.result;
            }
            if(call.operation==TimelineBoundary::AssetPlaying) {
                call.handled=invocation->call.handled;call.boolean_result=invocation->call.boolean_result;
            }
            if(call.operation==TimelineBoundary::Return || call.operation==TimelineBoundary::RecentCompletion ||
                call.operation==TimelineBoundary::StopName || call.operation==TimelineBoundary::ResourceLookup || call.operation==TimelineBoundary::AssetPlaying)
                call.receiver=invocation->call.receiver;
            if(call.operation==TimelineBoundary::RecentCompletion) {
                call.handled=invocation->call.handled;call.boolean_result=invocation->call.boolean_result;
            }
            if(call.operation==TimelineBoundary::StopName) {
                call.handled=invocation->call.handled;call.stop_record=invocation->call.stop_record;
            }
            invocation->call=call;
            return invocation.release();
        }catch(const std::exception& e) {state->Log(std::string("begin; original arguments forwarded: ")+e.what());return nullptr;}
        catch(...) {state->Log("begin; original arguments forwarded: unknown exception");return nullptr;}
    }
    static void Leave(Invocation* raw,void* result,bool abnormal) noexcept {
        if(!raw)return;
        std::unique_ptr<Invocation> invocation(raw);
        try{invocation->state->end(invocation->receipt,invocation->call,result,abnormal);}
        catch(const std::exception& e){invocation->state->Log(std::string("end; original outcome preserved: ")+e.what());}
        catch(...){invocation->state->Log("end; original outcome preserved: unknown exception");}
    }
    using BorrowFn=void*(__fastcall*)(void*,void*,void*,void*,const void*,const void*,void*);
    static void* __fastcall Borrow(void* self,void* name,void* resource,void* asset,const void* position,const void* rotation,void* method) {
        TimelineBoundaryCall call{TimelineBoundary::Borrow,self,name,resource,asset,position,rotation};
        auto* invocation=Enter(call);void* result=nullptr;
        __try {result=reinterpret_cast<BorrowFn>(Route().hooks.Original(0))(self,call.argument,resource,asset,position,rotation,method);}
        __finally {Leave(invocation,result,AbnormalTermination()!=FALSE);}
        return result;
    }
    using ObjectFn=void(__fastcall*)(void*,void*,void*);
    template<TimelineBoundary Op,std::size_t Index>static void __fastcall Object(void* self,void* object,void* method) {
        TimelineBoundaryCall call{Op,self,object};auto* invocation=Enter(call);
        __try {
            if constexpr(Op==TimelineBoundary::StopName) {
                if(!call.handled)reinterpret_cast<ObjectFn>(Route().hooks.Original(Index))(call.receiver?call.receiver:self,call.argument,method);
                else if(call.stop_record)reinterpret_cast<void(__fastcall*)(void*,void*)>(Route().stop_record.code)(call.stop_record,Route().stop_record.metadata);
            }else reinterpret_cast<ObjectFn>(Route().hooks.Original(Index))(call.receiver?call.receiver:self,call.argument,method);
        }
        __finally{Leave(invocation,nullptr,AbnormalTermination()!=FALSE);}
    }
    static void __fastcall Dispose(void* self,void* method) {
        TimelineBoundaryCall call{TimelineBoundary::Dispose,self};auto* invocation=Enter(call);
        __try{reinterpret_cast<void(__fastcall*)(void*,void*)>(Route().hooks.Original(3))(self,method);}
        __finally{Leave(invocation,nullptr,AbnormalTermination()!=FALSE);}
    }
    static void* __fastcall Resource(void* self,void* path,void* method) {
        TimelineBoundaryCall call{TimelineBoundary::ResourceLookup,self,path};auto* invocation=Enter(call);
        void* result=nullptr;
        __try {
            result=call.handled?call.result:
                reinterpret_cast<void*(__fastcall*)(void*,void*,void*)>(Route().hooks.Original(5))(call.receiver?call.receiver:self,path,method);
        }__finally{Leave(invocation,result,AbnormalTermination()!=FALSE);}
        return result;
    }
    static bool __fastcall AssetPlaying(void* self,void* name,void* method) {
        TimelineBoundaryCall call{TimelineBoundary::AssetPlaying,self,name};auto* invocation=Enter(call);
        bool result=false;
        __try {
            result=call.handled?call.boolean_result:
                reinterpret_cast<bool(__fastcall*)(void*,void*,void*)>(Route().hooks.Original(6))(call.receiver?call.receiver:self,name,method);
        }__finally{Leave(invocation,nullptr,AbnormalTermination()!=FALSE);}
        return result;
    }
    static bool __fastcall Recent(void* self,void* name,std::int32_t frames,void* method) {
        TimelineBoundaryCall call{TimelineBoundary::RecentCompletion,self,name};call.frame_window=frames;
        auto* invocation=Enter(call);bool result=false;
        __try {
            result=call.handled?call.boolean_result:
                reinterpret_cast<bool(__fastcall*)(void*,void*,std::int32_t,void*)>(Route().hooks.Original(7))(
                    call.receiver?call.receiver:self,name,frames,method);
        }__finally{Leave(invocation,nullptr,AbnormalTermination()!=FALSE);}
        return result;
    }
};
struct TimelineRegistryHooks::Holder{std::shared_ptr<State> state;};
TimelineRegistryHooks::TimelineRegistryHooks(MethodResolver& m,Begin begin,End end,Trace trace) {
    if(!begin || !end)throw std::invalid_argument("Timeline boundary needs both transaction callbacks");
    auto& r=State::Route();std::lock_guard<std::mutex> lock(r.install);
    if(std::atomic_load(&r.current))throw std::logic_error("Timeline boundary already owned");
    if(r.hooks.Pending() && !r.installed)
        throw std::logic_error("Timeline hook group has an unresolved partial installation; cannot activate incomplete boundaries");
    auto state=std::make_shared<State>(std::move(begin),std::move(end),std::move(trace));
    if(!r.hooks.Pending()) {
        const auto [borrow,returned,removed,disposed,stop,resource,playing,recent,stop_record]=ResolveTimelineBoundaryMethods(m);
        r.stop_record=stop_record;
        r.hooks.Install({{"Timeline borrow",borrow.code,reinterpret_cast<void*>(&State::Borrow)},
            {"Timeline pool return",returned.code,reinterpret_cast<void*>(&State::Object<TimelineBoundary::Return,1>)},
            {"Timeline record removal",removed.code,reinterpret_cast<void*>(&State::Object<TimelineBoundary::Remove,2>)},
            {"Timeline registry disposal",disposed.code,reinterpret_cast<void*>(&State::Dispose)},
            {"Timeline runtime-name stop",stop.code,reinterpret_cast<void*>(&State::Object<TimelineBoundary::StopName,4>)},
            {"Timeline resource lookup",resource.code,reinterpret_cast<void*>(&State::Resource)},
            {"Timeline asset-name playing",playing.code,reinterpret_cast<void*>(&State::AssetPlaying)},
            {"Timeline recent completion",recent.code,reinterpret_cast<void*>(&State::Recent)}});
        r.installed=true;
    }
    holder_=new Holder{state};std::atomic_store(&r.current,std::move(state));
}
TimelineRegistryHooks::~TimelineRegistryHooks(){Stop();}
void TimelineRegistryHooks::ActivateOnCurrentThread() {
    if(!holder_)return;
    const auto id=GetCurrentThreadId();DWORD empty=0;
    if(!holder_->state->thread.compare_exchange_strong(empty,id) && empty!=id)
        throw std::logic_error("Timeline boundaries cannot migrate engine threads");
}
void TimelineRegistryHooks::Stop() noexcept {
    if(!holder_)return;
    try {
        auto& r=State::Route();std::lock_guard<std::mutex> lock(r.install);
        if(std::atomic_load(&r.current)==holder_->state)std::atomic_store(&r.current,std::shared_ptr<State>{});
        holder_->state->thread.store(0);delete holder_;holder_=nullptr;
    }catch(...){holder_->state->thread.store(0);}
}
}
