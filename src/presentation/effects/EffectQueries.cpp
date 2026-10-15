#include "EffectQueries.hpp"
#include <hksr/native/MetadataReader.hpp>
#include <hksr/native/Invocation.hpp>
#include "../../platform/NativeHooks.hpp"
#include <hksr/managed/ManagedLists.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <Windows.h>
#include <atomic>
#include <mutex>

namespace hksr::sdk_v1::detail {
namespace {
constexpr auto ListType="System.Collections.Generic.List<RPG.Client.MonoEffect>";
std::size_t Field(const std::vector<DeclaredField>& fields,const char* owner,const char* name,const char* type) {
    const DeclaredField* found=nullptr;
    for(const auto& f:fields)if(f.owner_namespace=="RPG.Client" && f.owner_name==owner && f.name==name) {
        if(found || f.type!=type || !f.flags_observed || (f.flags&0x10))
            throw std::invalid_argument(std::string("effect query field differs: ")+owner+"."+name);
        found=&f;
    }
    if(!found)throw std::invalid_argument(std::string("effect query field absent: ")+owner+"."+name);
    return found->offset;
}
template<class T>T Value(void* object,std::size_t offset) {
    T value{};
    if(!object || !native::CopyMemoryChecked(&value,static_cast<unsigned char*>(object)+offset,sizeof(value)))
        throw std::runtime_error("effect query field unreadable");
    return value;
}
}
struct EffectQueries::State {
    struct Router {NativeHooks hooks;std::mutex install;std::shared_ptr<State> current;bool installed=false;};
    static Router& Route(){static auto* value=new Router;return *value;}
    MethodResolver& methods;ObjectMetadata& metadata;ManagedObjects& objects;
    ReferenceMethods references;EffectRelations relations;Source source;Trace trace;
    Method clone,get_field;std::size_t list_offset,request_offset,id_offset;
    std::atomic<DWORD> thread{0};bool reported_unknown=false;
    struct Invocation {
        std::shared_ptr<State> state;ManagedRoots roots;std::vector<ManagedRootId> held;
        void* receiver=nullptr;bool projected=false;
        Invocation(std::shared_ptr<State> s):state(std::move(s)),roots(state->references){}
        void Keep(void* object) {
            if(!object)return;
            held.reserve(held.size()+1);const auto id=roots.Keep(object);held.push_back(id);
            if(!roots.Status(id).ready)throw std::runtime_error("effect query GC retention failed");
        }
        bool Close() {
            bool complete=true;
            for(auto id:held) {
                if(roots.Release(id).complete)roots.Forget(id);else complete=false;
            }
            held.clear();return complete;
        }
        void* Require(ObjectResult result) {
            if(!result.call.returned || !result.object)throw std::runtime_error("effect query reflection unavailable");
            Keep(result.object);return result.object;
        }
    };
    State(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,ReferenceMethods refs,
        const TimelineRegistryAccess::Fields& fields,Source s,Trace t)
        :methods(m),metadata(d),objects(o),references(refs),
        relations(fields("RPG.Client","TriggerEffectParams")),source(std::move(s)),trace(std::move(t)) {
        list_offset=Field(fields("RPG.Client","MonoEffectManager"),"MonoEffectManager","_EffectList",ListType);
        request_offset=Field(fields("RPG.Client","MonoEffect"),"MonoEffect","TriggerParams","RPG.Client.TriggerEffectParams");
        id_offset=Field(fields("RPG.Client","TriggerEffectParams"),"TriggerEffectParams","<UniqueEffectID>k__BackingField","System.UInt32");
        clone=m.Resolve("System","Object","MemberwiseClone","System.Object",{});
        get_field=m.Resolve("System","RuntimeType","GetField","System.Reflection.FieldInfo",{"System.String","System.Reflection.BindingFlags"});
    }
    void Log(const std::string& message) noexcept {try{if(trace)trace("Effect query: "+message);}catch(...) {}}
    void Project(Invocation& inv,void* manager,void* name,void* target) {
        // Both missing scope and an explicit unknown barrier preserve the
        // original call, but neither is mislabeled as an official invocation.
        if(!TimelineCallScope::Active() || !TimelineCallScope::Established()) {
            if(!reported_unknown){Log("caller source unresolved; original query forwarded (further occurrences suppressed)");reported_unknown=true;}
            return;
        }
        if(!manager || !name)return; // preserve original null-input behavior
        const auto caller=TimelineCallScope::Current();
        inv.Keep(manager);inv.Keep(name);inv.Keep(target);
        auto* list=Value<void*>(manager,list_offset);
        if(!list)return;
        inv.Keep(list);const auto type=metadata.ReadObjectType(list);MetadataResolver resolver({type});
        ManagedReferenceList access(ResolveReferenceListMethods(type,"RPG.Client.MonoEffect"));
        const auto count=access.Count(list);
        if(!count.read.returned || count.value<0)throw std::runtime_error("effect list count unreadable");
        if(!count.value)return;
        std::vector<std::int32_t> hidden;
        for(std::int32_t i=count.value;i-->0;) {
            const auto row=access.Item(list,i);
            if(!row.call.returned)throw std::runtime_error("effect query row unreadable");
            if(!row.object)continue; // original skips null rows
            auto* request=Value<void*>(row.object,request_offset);
            if(!request)continue; // original cannot return this row
            inv.Keep(request);
            const EffectReference ref{Value<std::uint32_t>(request,id_offset),0,request};
            const auto facts=relations.Read(ref);
            if(!facts.same_generation || !facts.caster.call.returned)
                throw std::runtime_error("effect request source unreadable");
            inv.Keep(facts.caster.object);
            const auto owner=source(ref,facts);
            // Source callback may use weak-handle reads, but must not manipulate
            // effects. Detect reuse before publishing this temporary view.
            if(Value<void*>(row.object,request_offset)!=request || Value<std::uint32_t>(request,id_offset)!=ref.id)
                throw std::runtime_error("effect request changed during source resolution");
            if(owner==caller)continue;
            hidden.push_back(i);
        }
        if(hidden.empty())return; // no managed copies when nothing is hidden
        // Independent backing storage. Reverse deletion preserves order. No
        // game mutations in source resolution; no user callbacks in list access.
        const auto range=resolver.Resolve(type.name_space.c_str(),type.name.c_str(),"GetRange",ListType,{"System.Int32","System.Int32"});
        void* filtered=nullptr;
        if(!native::Call(range,&filtered,list,0,count.value).returned || !filtered || filtered==list)
            throw std::runtime_error("effect query list copy failed");
        inv.Keep(filtered);
        const auto remove=resolver.Resolve(type.name_space.c_str(),type.name.c_str(),"RemoveAt","System.Void",{"System.Int32"});
        for(const auto i:hidden)if(!native::CallVoid(remove,filtered,i).returned)
            throw std::runtime_error("effect query list filtering failed");
        void* view=nullptr;
        if(!native::Call(clone,&view,manager).returned || !view || view==manager)
            throw std::runtime_error("effect query receiver copy failed");
        inv.Keep(view);
        auto* managed_type=inv.Require(objects.ManagedType("RPG.Client","MonoEffectManager"));
        auto* field_name=inv.Require(objects.NewStringUtf8("_EffectList"));
        ObjectResult field_result;field_result.call=native::Call(get_field,&field_result.object,managed_type,field_name,54);
        auto* field=inv.Require(field_result);const auto field_type=metadata.ReadObjectType(field);MetadataResolver fields({field_type});
        const auto set=fields.Resolve(field_type.name_space.c_str(),field_type.name.c_str(),"SetValue","System.Void",
            {"System.Object","System.Object","System.Reflection.BindingFlags","System.Reflection.Binder","System.Globalization.CultureInfo"});
        if(!native::CallVoid(set,field,view,filtered,0,static_cast<void*>(nullptr),static_cast<void*>(nullptr)).returned ||
            Value<void*>(view,list_offset)!=filtered)
            throw std::runtime_error("effect query receiver assignment failed");
        inv.receiver=view;inv.projected=true;
    }
    static Invocation* Enter(void* manager,void* name,void* target) noexcept {
        const auto state=std::atomic_load(&Route().current);
        if(!state || state->thread.load()!=GetCurrentThreadId())return nullptr;
        std::unique_ptr<Invocation> inv;
        try {inv=std::make_unique<Invocation>(state);state->Project(*inv,manager,name,target);}
        catch(const std::exception& e){state->Log(std::string(e.what())+"; original query forwarded");}
        catch(...){state->Log("projection failed; original query forwarded");}
        return inv.release(); // cleanup is required even after partial preparation
    }
    static void Leave(Invocation* raw) noexcept {
        if(!raw)return;
        std::unique_ptr<Invocation> inv(raw);
        try {
            if(!inv->Close()) {inv->state->Log("GC release unresolved; receipt retained");inv.release();}
        }catch(...) {inv->state->Log("GC cleanup failed; receipt retained");inv.release();}
    }
    static void* __fastcall Find(void* manager,void* name,void* target,void* info) {
        auto* inv=Enter(manager,name,target);void* result=nullptr;
        __try {
            result=reinterpret_cast<void*(__fastcall*)(void*,void*,void*,void*)>(Route().hooks.Original(0))(
                inv && inv->projected?inv->receiver:manager,name,target,info);
        }__finally{Leave(inv);}
        return result;
    }
};
struct EffectQueries::Holder {std::shared_ptr<State> state;};
EffectQueries::EffectQueries(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,ReferenceMethods refs,
    TimelineRegistryAccess::Fields fields,Source source,Trace trace) {
    if(!source)throw std::invalid_argument("effect query source resolver required");
    auto& r=State::Route();std::lock_guard<std::mutex> lock(r.install);
    if(std::atomic_load(&r.current))throw std::logic_error("effect query adapter already owned");
    if(r.hooks.Pending() && !r.installed)throw std::logic_error("effect query hook installation incomplete");
    auto state=std::make_shared<State>(m,d,o,refs,fields,std::move(source),std::move(trace));
    if(!r.hooks.Pending()) {
        const auto find=m.Resolve("RPG.Client","MonoEffectManager","FindEffectByUniqueName","RPG.Client.MonoEffect",{"System.String","RPG.GameCore.GameEntity"});
        r.hooks.Install({{"effect name query",find.code,reinterpret_cast<void*>(&State::Find)}});r.installed=true;
    }
    holder_=new Holder{state};std::atomic_store(&r.current,std::move(state));
}
EffectQueries::~EffectQueries(){Stop();}
void EffectQueries::ActivateOnCurrentThread() {
    if(!holder_)return;
    DWORD empty=0;const auto id=GetCurrentThreadId();
    if(!holder_->state->thread.compare_exchange_strong(empty,id) && empty!=id)
        throw std::logic_error("effect query adapter cannot migrate engine threads");
}
void EffectQueries::Stop() noexcept {
    if(!holder_)return;
    try {
        auto& r=State::Route();std::lock_guard<std::mutex> lock(r.install);
        if(std::atomic_load(&r.current)==holder_->state)std::atomic_store(&r.current,std::shared_ptr<State>{});
        holder_->state->thread.store(0);delete holder_;holder_=nullptr;
    }catch(...) {}
}
}
