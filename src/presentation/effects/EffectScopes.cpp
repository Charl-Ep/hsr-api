#include "EffectScopes.hpp"
#include <hksr/native/MetadataReader.hpp>
#include <hksr/native/Invocation.hpp>
#include "../../platform/NativeHooks.hpp"
#include <hksr/managed/ManagedRoots.hpp>
#include <Windows.h>
#include <array>
#include <atomic>
#include <mutex>

namespace hksr::sdk_v1::detail {
namespace {
std::size_t Field(const std::vector<DeclaredField>& fields,const char* owner,const char* name,const char* type) {
    const DeclaredField* found=nullptr;
    for(const auto& f:fields)if(f.owner_namespace=="RPG.Client" && f.owner_name==owner && f.name==name) {
        if(found || f.type!=type || !f.flags_observed || (f.flags&0x10))throw std::invalid_argument("effect scope field declaration differs");
        found=&f;
    }
    if(!found)throw std::invalid_argument(std::string("effect scope field absent: ")+owner+"."+name);
    return found->offset;
}
template<class T>T Read(void* object,std::size_t offset) {
    T value{};
    if(!object || !native::CopyMemoryChecked(&value,static_cast<unsigned char*>(object)+offset,sizeof(value)))
        throw std::runtime_error("effect scope field unreadable");
    return value;
}
}
struct EffectScopes::State {
    struct Router {NativeHooks hooks;std::mutex install;std::shared_ptr<State> current;bool installed=false;};
    static Router& Route(){static auto* r=new Router;return *r;}
    ReferenceMethods refs;EffectRelations relations;EffectQueries::Source source;EffectQueries::Trace trace;
    Method owner_mono,camera_plugin;AllowMainCamera allow_camera;
    std::size_t plugin_owner,request_offset,id_offset;
    std::atomic<DWORD> thread{0};
    struct Invocation {
        std::shared_ptr<State> state;ManagedRoots roots;std::vector<ManagedRootId> pins;
        std::unique_ptr<TimelineCallScope> scope;
        explicit Invocation(std::shared_ptr<State> s):state(std::move(s)),roots(state->refs){}
        void Keep(void* p) {
            if(!p)return;
            pins.reserve(pins.size()+1);const auto id=roots.Keep(p);pins.push_back(id);
            if(!roots.Status(id).ready)throw std::runtime_error("effect scope GC retention failed");
        }
        bool Close() {
            scope.reset();bool complete=true;
            for(const auto id:pins) {
                if(roots.Release(id).complete)roots.Forget(id);else complete=false;
            }
            pins.clear();return complete;
        }
    };
    State(MethodResolver& m,ReferenceMethods r,const TimelineRegistryAccess::Fields& fields,
        EffectQueries::Source s,EffectQueries::Trace t,AllowMainCamera allow):refs(r),relations(fields("RPG.Client","TriggerEffectParams")),source(std::move(s)),trace(std::move(t)),allow_camera(std::move(allow)) {
        owner_mono=m.Resolve("RPG.Client","MonoEffectBehavior","get_OwnerMono","RPG.Client.MonoEffect",{});
        camera_plugin=m.Resolve("","JCNLLEPCBGI","HPNDIIPEFLJ","RPG.Client.MonoEffectPluginCtrlCamera",{});
        plugin_owner=Field(fields("RPG.Client","MonoEffectPluginBase"),"MonoEffectPluginBase","_OwnerEffect","RPG.Client.MonoEffect");
        request_offset=Field(fields("RPG.Client","MonoEffect"),"MonoEffect","TriggerParams","RPG.Client.TriggerEffectParams");
        id_offset=Field(fields("RPG.Client","TriggerEffectParams"),"TriggerEffectParams","<UniqueEffectID>k__BackingField","System.UInt32");
    }
    void Log(const std::string& text) noexcept {try{if(trace)trace("Effect scope: "+text);}catch(...) {}}
    enum class OwnerKind { Behavior, Plugin, CameraController };
    static Invocation* Enter(void* object,OwnerKind kind) noexcept {
        const auto state=std::atomic_load(&Route().current);
        if(!state || state->thread.load()!=GetCurrentThreadId())return nullptr;
        if(kind==OwnerKind::CameraController && !state->allow_camera)return nullptr;
        std::unique_ptr<Invocation> inv;
        try {
            inv=std::make_unique<Invocation>(state);
            // Even getter/source failure must not inherit a surrounding actor.
            inv->scope=std::make_unique<TimelineCallScope>(std::nullopt,false);
            inv->Keep(object);void* effect=nullptr;
            if(kind==OwnerKind::CameraController) {
                void* plugin=nullptr;
                if(!object || !native::Call(state->camera_plugin,&plugin,object).returned || !plugin)
                    throw std::runtime_error("camera plugin getter failed");
                inv->Keep(plugin);effect=Read<void*>(plugin,state->plugin_owner);
            }
            else if(kind==OwnerKind::Plugin)effect=Read<void*>(object,state->plugin_owner);
            else if(!object || !native::Call(state->owner_mono,&effect,object).returned)
                throw std::runtime_error("effect owner getter failed");
            if(!effect)throw std::runtime_error("effect owner absent");
            inv->Keep(effect);auto* request=Read<void*>(effect,state->request_offset);
            if(!request)throw std::runtime_error("effect request absent");
            inv->Keep(request);const EffectReference ref{Read<std::uint32_t>(request,state->id_offset),0,request};
            const auto facts=state->relations.Read(ref);
            if(!facts.same_generation || !facts.caster.call.returned)throw std::runtime_error("effect source unreadable");
            inv->Keep(facts.caster.object);const auto owner=state->source(ref,facts);
            if(Read<void*>(effect,state->request_offset)!=request || Read<std::uint32_t>(request,state->id_offset)!=ref.id)
                throw std::runtime_error("effect request changed during source resolution");
            inv->scope.reset();inv->scope=std::make_unique<TimelineCallScope>(owner);
        }catch(const std::exception& e){state->Log(e.what());}
        catch(...){state->Log("source resolution failed");}
        return inv.release();
    }
    static void Leave(Invocation* raw) noexcept {
        if(!raw)return;
        std::unique_ptr<Invocation> inv(raw);
        try{if(!inv->Close()){inv->state->Log("GC cleanup unresolved; receipts retained");inv.release();}}
        catch(...){inv->scope.reset();inv->state->Log("GC cleanup failed; receipts retained");inv.release();}
    }
    template<std::size_t Index,bool Plugin=false>static void __fastcall Void(void* object,void* info) {
        auto* inv=Enter(object,Plugin?OwnerKind::Plugin:OwnerKind::Behavior);
        __try{reinterpret_cast<void(__fastcall*)(void*,void*)>(Route().hooks.Original(Index))(object,info);}
        __finally{Leave(inv);}
    }
    static void __fastcall Tick(void* object,float delta,void* info) {
        auto* inv=Enter(object,OwnerKind::Behavior);
        __try{reinterpret_cast<void(__fastcall*)(void*,float,void*)>(Route().hooks.Original(3))(object,delta,info);}
        __finally{Leave(inv);}
    }
    static bool BlockCamera(Invocation* inv) noexcept {
        if(!inv || !TimelineCallScope::Established())return false;
        try {
            const auto owner=TimelineCallScope::Current();
            return owner && inv->state->allow_camera && !inv->state->allow_camera(*owner);
        }catch(...){inv->state->Log("main-camera policy failed; original forwarded");return false;}
    }
    static void __fastcall CameraEnabled(void* object,bool enabled,void* info) {
        // Never suppress cleanup, including a retired/cleared owner. Original
        // false is a no-op when already disabled, otherwise unsubscribes itself.
        auto* inv=enabled?Enter(object,OwnerKind::CameraController):nullptr;
        __try{reinterpret_cast<void(__fastcall*)(void*,bool,void*)>(Route().hooks.Original(5))(
            object,enabled && !BlockCamera(inv),info);}
        __finally{Leave(inv);}
    }
    static void __fastcall CameraUpdate(void* object,void* info) {
        auto* inv=Enter(object,OwnerKind::CameraController);
        __try {
            // The original update writes main-camera properties AFTER calling
            // enable(true), without checking the enable flag. Enable filtering
            // alone is insufficient. Preserve cleanup using the original ABI.
            if(BlockCamera(inv))reinterpret_cast<void(__fastcall*)(void*,bool,void*)>(
                Route().hooks.Original(5))(object,false,inv->state->camera_enable_info);
            else reinterpret_cast<void(__fastcall*)(void*,void*)>(Route().hooks.Original(6))(object,info);
        }__finally{Leave(inv);}
    }
    void* camera_enable_info=nullptr;
};
struct EffectScopes::Holder {std::shared_ptr<State> state;};
EffectScopes::EffectScopes(MethodResolver& m,ReferenceMethods refs,TimelineRegistryAccess::Fields fields,
    EffectQueries::Source source,EffectQueries::Trace trace,AllowMainCamera allow) {
    if(!source)throw std::invalid_argument("effect scope source resolver required");
    auto& r=State::Route();std::lock_guard<std::mutex> lock(r.install);
    if(std::atomic_load(&r.current))throw std::logic_error("effect scopes already owned");
    if(r.hooks.Pending() && !r.installed)throw std::logic_error("effect scope installation incomplete");
    auto state=std::make_shared<State>(m,refs,fields,std::move(source),std::move(trace),std::move(allow));
    const auto camera_enable=m.Resolve("","JCNLLEPCBGI","NMIPDBNGLBH","System.Void",{"System.Boolean"});
    state->camera_enable_info=camera_enable.metadata;
    if(!r.hooks.Pending()) {
        const std::array<Method,7> entries{{
            m.Resolve("RPG.Client","MonoEffectBehavior","PreResetPlugins","System.Void",{}),
            m.Resolve("RPG.Client","MonoEffectBehavior","ResetPlugins","System.Void",{}),
            m.Resolve("RPG.Client","MonoEffectBehavior","DisposePlugins","System.Void",{}),
            m.Resolve("RPG.Client","MonoEffectBehavior","Tick","System.Void",{"System.Single"}),
            m.Resolve("RPG.Client","MonoEffectFixedCRPPlugin","EFPBCKGBFBG","System.Void",{}),
            camera_enable,m.Resolve("","JCNLLEPCBGI","BLOMJCEJLFG","System.Void",{})}};
        r.hooks.Install({
            {"effect pre-reset plugins",entries[0].code,reinterpret_cast<void*>(&State::Void<0>)},
            {"effect reset plugins",entries[1].code,reinterpret_cast<void*>(&State::Void<1>)},
            {"effect dispose plugins",entries[2].code,reinterpret_cast<void*>(&State::Void<2>)},
            {"effect tick",entries[3].code,reinterpret_cast<void*>(&State::Tick)},
            {"effect fixed CRP query",entries[4].code,reinterpret_cast<void*>(&State::Void<4,true>)},
            {"effect main camera enable",entries[5].code,reinterpret_cast<void*>(&State::CameraEnabled)},
            {"effect main camera update",entries[6].code,reinterpret_cast<void*>(&State::CameraUpdate)}});r.installed=true;
    }
    holder_=new Holder{state};std::atomic_store(&r.current,std::move(state));
}
EffectScopes::~EffectScopes(){Stop();}
void EffectScopes::ActivateOnCurrentThread() {
    if(!holder_)return;
    DWORD empty=0;const auto id=GetCurrentThreadId();
    if(!holder_->state->thread.compare_exchange_strong(empty,id) && empty!=id)throw std::logic_error("effect scopes cannot migrate engine threads");
}
void EffectScopes::Stop() noexcept {
    if(!holder_)return;
    try {
        auto& r=State::Route();std::lock_guard<std::mutex> lock(r.install);
        if(std::atomic_load(&r.current)==holder_->state)std::atomic_store(&r.current,std::shared_ptr<State>{});
        holder_->state->thread.store(0);delete holder_;holder_=nullptr;
    }catch(...) {holder_->state->thread.store(0);}
}
}
