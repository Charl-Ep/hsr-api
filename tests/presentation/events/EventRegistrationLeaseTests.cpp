#include <hksr/presentation/events/EventRegistrationLease.hpp>
#include <Windows.h>
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<hksr::api::EventRegistrationLease>);
static_assert(!std::is_copy_assignable_v<hksr::api::EventRegistrationLease>);

namespace {
using namespace hksr;
using Objects=std::vector<void*>;
int info,entity,foreign_entity,consumer,next_consumer,sequence=0;
void* caster=&entity;
std::map<int,void*> handles;
std::map<void*,int> registered,detached,disposed;
struct Context {} context,other_context;
struct Component { void* unused[4]; void* consumer=&::consumer; Context* context=&::context; } component;
Objects records,baseline,desired;
std::wstring common=L"共有",common_copy=L"共有",one=L"事件/表情",one_copy=L"事件/表情",two=L"事件/附件";
int first_record,second_record,baseline_record;
void* fault_register=nullptr;
void* fault_detach=nullptr;
void* fault_dispose=nullptr;
int reads=0,writes=0;
bool list_fault=false,end_on_register=false,end_on_detach=false,consumer_on_register=false,dispose_on_detach=false;
bool nested_dispose_fault=false;
api::EventRegistrationLease* active=nullptr;
void Check(bool ok) { if(!ok) throw std::runtime_error("event input consumer test failed"); }
template<class T> api::Method M(T function) { return {reinterpret_cast<void*>(function),&info}; }
int __fastcall Keep(void* value,int handle,int kind,void*) { Check(!handle && kind==2); handles[++sequence]=value; return sequence; }
void* __fastcall Target(int handle,void*) { return handles.at(handle); }
void __fastcall Free(int handle,void*) { handles.erase(handle); }
void* __fastcall ContextOf(Component* value,void* metadata) { Check(value==&component && metadata==&info); ++reads; return value->context; }
void* __fastcall Caster(void*,void*) { ++reads; return caster; }
void* __fastcall Records(Component* value,void*) { Check(value==&component); ++reads; return &records; }
int __fastcall Count(Objects* value,void*) {
    ++reads;
    if(list_fault && value==&records) RaiseException(0xE0670004,0,0,nullptr);
    return static_cast<int>(value->size());
}
void* __fastcall Item(Objects* value,int index,void*) { ++reads; return value->at(index); }
bool __fastcall Equal(std::wstring* first,std::wstring* second,void*) { return *first==*second; }
void* __fastcall Register(Component* value,void* ctx,void* path,void* metadata) {
    Check(value==&component && ctx==component.context && metadata==&info); ++writes; ++registered[path];
    void* record=path==&one?&first_record:&second_record; records.push_back(record);
    if(end_on_register) active->OwnerEnded();
    if(consumer_on_register) component.consumer=&next_consumer;
    if(fault_register==path) RaiseException(0xE0670001,0,0,nullptr);
    return record;
}
void* __fastcall RegisterInline(Component* value,void* ctx,void* single,void* behaviors,void* groups,void* metadata) {
    Check(!single && !behaviors && groups==&one);return Register(value,ctx,groups,metadata);
}
__declspec(noinline) void __fastcall Dispose(void* record,void*);
void __fastcall Detach(Component* value,void* record,bool option,void*) {
    Check(value==&component && option && record!=&baseline_record); ++writes; ++detached[record];
    records.erase(std::remove(records.begin(),records.end(),record),records.end());
    if(end_on_detach) active->OwnerEnded();
    if(fault_detach==record) RaiseException(0xE0670002,0,0,nullptr);
    if(dispose_on_detach) Dispose(record,&info);
}
__declspec(noinline) void __fastcall Dispose(void* record,void*) {
    Check(record!=&baseline_record); ++writes; ++disposed[record];
    if(nested_dispose_fault) {
        nested_dispose_fault=false;
        void(__fastcall* volatile nested)(void*,void*)=&Dispose;
        nested(record,&info);
        RaiseException(0xE0670005,0,0,nullptr);
    }
    if(fault_dispose==record) RaiseException(0xE0670003,0,0,nullptr);
}
api::ObjectResult Ref(void* value) { return {{true,0},value}; }
void Reset() {
    Check(handles.empty()); registered.clear(); detached.clear(); disposed.clear();
    caster=&entity; component.context=&context; component.consumer=&consumer;
    records={&baseline_record}; baseline={&common}; desired={&common_copy,&one,&one_copy,&two};
    fault_register=fault_detach=fault_dispose=nullptr;
    list_fault=end_on_register=end_on_detach=consumer_on_register=dispose_on_detach=false; reads=writes=0; active=nullptr;
    nested_dispose_fault=false;
}
struct LifetimeResolver : api::MethodResolver {
    api::Method Resolve(const char* ns,const char* owner,const char* name,const char* result,
        std::initializer_list<const char*> args,api::MethodKind kind) override {
        Check(std::string(ns).empty() && std::string(owner)=="LDMKDJJLOIL" && std::string(name)=="Dispose" &&
            std::string(result)=="System.Void" && args.size()==0 && kind==api::MethodKind::Instance);
        return M(Dispose);
    }
};
}
void TestEventRegistrationLease() {
    LifetimeResolver resolver; api::InstallEventRecordLifetime(resolver);
    const api::ReferenceMethods refs{M(Keep),M(Target),M(Free),{}, {}};
    api::AnimationEventMethods methods{M(ContextOf),M(Caster),M(Records),M(Register),M(Detach),M(Dispose),{}};
    methods.consumer_offset=offsetof(Component,consumer); methods.has_consumer=true;
    methods.register_inline=M(RegisterInline);
    const api::AnimationEvents events(methods);
    const api::ManagedArrays arrays({{},M(Count),M(Item),{}, {}});
    const api::ManagedReferenceList lists({M(Count),M(Item),{}});
    const api::SceneObjects strings({{},{},{},M(Equal),{}});
    auto prepare=[&](api::EventRegistrationLease& input) {
        active=&input; input.Prepare(arrays,strings,Ref(&baseline),Ref(&desired));
        Check(input.Status().prepared && input.Files().size()==2);
        input.Attach(&entity,&component); Check(input.Status().attached);
    };
    Reset();
    {
        api::EventRegistrationLease input(refs,events,lists);active=&input;
        input.PrepareInline(&one);input.Attach(&entity,&component);input.Service();input.Service();
        Check(input.Status().prepared && registered[&one]==1 && input.Files()[0].present);
        Check(input.Retire().complete && detached[&first_record]==1 && disposed[&first_record]==1);
        Check(input.Close().complete && handles.empty() && records==Objects{&baseline_record});
    }
    Reset();
    {
        api::EventRegistrationLease input(refs,events,lists); prepare(input); input.Service(); input.Service();
        Check(registered.size()==2 && registered[&one]==1 && registered[&two]==1);
        Check(input.Files()[0].present && input.Files()[1].present);
        Check(input.Retire().complete && input.Status().retirement_complete);
        const auto before=writes; const auto read_before=reads;
        Check(input.Retire().complete && writes==before && reads==read_before);
        Check(records==Objects{&baseline_record} && input.Close().complete && handles.empty());
    }
    Reset();
    {
        api::EventRegistrationLease input(refs,events,lists); prepare(input); fault_register=&one;
        input.Service(); input.Service();
        Check(registered[&one]==1 && registered[&two]==1 && input.Files()[0].registration.call.exception==0xE0670001);
        Check(!input.Retire().complete && disposed[&second_record]==1 && disposed[&first_record]==0);
        const auto before=writes; Check(!input.Retire().complete && writes==before && !input.Close().complete);
        input.OwnerEnded(); const auto read_before=reads; Check(input.Close().complete && reads==read_before && writes==before);
    }
    Reset();
    {
        api::EventRegistrationLease input(refs,events,lists); prepare(input); input.Service(); fault_detach=&first_record;
        Check(!input.Retire().complete && detached[&first_record]==1 && disposed[&first_record]==0 && disposed[&second_record]==1);
        const auto before=writes; fault_detach=nullptr; Check(!input.Retire().complete && writes==before);
        input.OwnerEnded(); Check(input.Close().complete);
    }
    Reset();
    {
        api::EventRegistrationLease input(refs,events,lists); prepare(input); input.Service(); fault_dispose=&first_record;
        Check(!input.Retire().complete && disposed[&first_record]==1 && disposed[&second_record]==1);
        const auto before=writes; fault_dispose=nullptr; Check(!input.Retire().complete && writes==before);
        input.OwnerEnded(); Check(input.Close().complete);
    }
    Reset();
    {
        api::EventRegistrationLease input(refs,events,lists); prepare(input); input.Service(); component.consumer=&next_consumer;
        const auto before=writes; input.Service(); Check(input.Status().identity_changed && writes==before);
        Check(!input.Retire().complete && writes==before && input.Close().complete && !input.Status().retirement_complete);
    }
    Reset();
    {
        api::EventRegistrationLease input(refs,events,lists); prepare(input); input.Service(); records={&baseline_record};
        input.Service(); Check(registered[&one]==1 && registered[&two]==1 && !input.Files()[0].present);
        Check(!input.Retire().complete && disposed.empty()); input.OwnerEnded(); Check(input.Close().complete);
    }
    Reset();
    {
        api::EventRegistrationLease input(refs,events,lists); active=&input;
        input.Prepare(arrays,strings,Ref(&baseline),Ref(&desired)); caster=&foreign_entity;
        input.Attach(&entity,&component); Check(!input.Status().attached); input.Service(); Check(writes==0 && input.Close().complete);
    }
    Reset();
    {
        api::EventRegistrationLease input(refs,events,lists); prepare(input); list_fault=true;
        input.Service(); Check(registered.size()==2); // unrelated list observation cannot gate first registration
        input.Service(); Check(!input.Files()[0].observed);
        list_fault=false; input.Service(); Check(input.Files()[0].observed && registered[&one]==1);
        input.OwnerEnded(); const auto before=reads; Check(input.Close().complete && reads==before);
    }
    Reset();
    {
        api::EventRegistrationLease input(refs,events,lists); prepare(input); end_on_register=true; input.Service();
        Check(registered.size()==1 && input.Status().owner_ended && input.Close().complete);
    }
    Reset();
    {
        api::EventRegistrationLease input(refs,events,lists); prepare(input); input.Service(); end_on_detach=true;
        Check(input.Retire().complete && detached.size()==1 && disposed.empty() && !input.Status().retirement_complete);
        Check(input.Close().complete);
    }
    Reset();
    {
        api::EventRegistrationLease input(refs,events,lists); prepare(input); consumer_on_register=true; input.Service();
        Check(registered.size()==1 && input.Status().identity_changed && input.Close().complete);
    }
    Reset();
    {
        api::EventRegistrationLease input(refs,events,lists); prepare(input); input.Service();
        const auto old=input.Files()[0].lifetime;
        Detach(&component,&first_record,true,&info); Dispose(&first_record,&info);
        Check(api::ObserveEventRecord(old).returned);
        input.Service(); Check(registered[&one]==2 && registered[&two]==1 && input.Files()[0].previous.size()==1);
        Check(input.Files()[0].previous[0].official_end.returned && !api::ObserveEventRecord(old).tracked);
        Check(input.Retire().complete && input.Close().complete);
    }
    Reset();
    {
        api::EventRegistrationLease input(refs,events,lists); prepare(input); input.Service();
        Detach(&component,&first_record,true,&info); fault_dispose=&first_record;
        Check(events.Dispose(&first_record).exception==0xE0670003); fault_dispose=nullptr;
        input.Service(); Check(registered[&one]==1 && input.Files()[0].official_end.entered && !input.Files()[0].official_end.returned);
        Check(!input.Retire().complete && disposed[&first_record]==1 && disposed[&second_record]==1);
        input.OwnerEnded(); Check(input.Close().complete);
    }
    Reset();
    {
        api::EventRegistrationLease input(refs,events,lists); prepare(input); input.Service();
        Detach(&component,&first_record,true,&info); nested_dispose_fault=true;
        Check(events.Dispose(&first_record).exception==0xE0670005);
        input.Service(); Check(input.Files()[0].official_end.entered && !input.Files()[0].official_end.returned && registered[&one]==1);
        Check(!input.Retire().complete && disposed[&first_record]==2 && disposed[&second_record]==1);
        input.OwnerEnded(); Check(input.Close().complete);
    }
    Reset();
    {
        api::EventRegistrationLease input(refs,events,lists); prepare(input); input.Service(); dispose_on_detach=true;
        Check(input.Retire().complete && disposed[&first_record]==1 && disposed[&second_record]==1);
        Check(input.Close().complete);
    }
    Reset();
    {
        auto no_list=methods; no_list.records={};
        api::EventRegistrationLease input(refs,api::AnimationEvents(no_list),lists); prepare(input);
        input.Service(); Check(registered.size()==2);
        Check(!input.Retire().complete && detached.empty()); input.OwnerEnded(); Check(input.Close().complete);
    }
    Reset();
    api::RemoveEventRecordLifetimeAtQuiescentPoint();
    {
        api::EventRegistrationLease input(refs,events,lists); input.Prepare(arrays,strings,Ref(nullptr),Ref(nullptr));
        Check(input.Status().prepared && input.Files().empty() && input.Retire().complete && input.Close().complete);
    }
    Reset();
    std::cout<<"Event input consumers passed: Unicode path difference, per-file calls, official identity, independent retirement and natural end; local substitutes only.\n";
}
