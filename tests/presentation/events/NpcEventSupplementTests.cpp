#include <hksr/presentation/events/NpcEventSupplement.hpp>
#include <hksr/presentation/PresentationContext.hpp>
#include <hksr/presentation/PresentationBindings.hpp>
#include <hksr/presentation/events/NpcEventBindings.hpp>
#include <hksr/entity/EntityCreationSession.hpp>
#include <Windows.h>
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
using namespace hksr;
using Values=std::vector<void*>;
int info,world_value,manager,definition,row,entity,data_type,event_type,sequence=0;
api::GameWorld* world=reinterpret_cast<api::GameWorld*>(&world_value);
api::GameEntity* own=reinterpret_cast<api::GameEntity*>(&entity);
std::wstring shared=L"共有",first=L"表情",second=L"附件";
Values baseline{&shared},desired{&shared,&first,&second},other_baseline{&shared,&first};
struct Config { void* padding[3]{}; void* events=&baseline;void* performance=&baseline; } config;
struct Data { void* padding[5]{}; void* config=&::config; } data;
struct Context { void* caster=own; } context,other_context;
struct Component { void* unused[3]{}; void* consumer=nullptr; Context* context=&::context; Values records; } component,other_component;
int consumer,next_consumer;
Component* current=&component;
std::map<int,void*> handles;
std::vector<std::unique_ptr<int>> record_storage;
unsigned reads=0,registrations=0,detaches=0,disposals=0,retirements=0;
bool loaded=true,dispose_on_register=false;
bool free_fault=false;
void Check(bool value) { if(!value) throw std::runtime_error("NPC event integration failed"); }
template<class T> api::Method M(T f) { return {reinterpret_cast<void*>(f),&info}; }
api::ObjectResult Ref(void* p) { return {{true,0},p}; }
int __fastcall Keep(void* value,int zero,int kind,void*) { Check(!zero && kind==2); handles[++sequence]=value; return sequence; }
void* __fastcall Target(int id,void*) { return handles.at(id); }
void __fastcall Free(int id,void*) {
    handles.erase(id);
    if(free_fault) { free_fault=false; RaiseException(0xE06D0001,0,0,nullptr); }
}
api::GameEntity* __fastcall Create(void*,void*,void*,void* callback,bool,void*) { Check(!callback); return own; }
api::GameEntity* __fastcall Story(void*,int,void*,void*,void*,void*,void*,void*,void*,void*,bool,void*,void*) {
    api::EntityCreationScope::FactoryCompleted(world,own);return own;
}
std::uint32_t __fastcall Id(void* p,void*) { Check(p==own); ++reads; return 7; }
api::GameWorld* __fastcall World(void* p,void*) { Check(p==own); ++reads; return world; }
bool __fastcall Loaded(void* p,void*) { Check(p==own); ++reads; return loaded; }
void* __fastcall ComponentOf(void* p,void* type,void*) {
    Check(p==own); ++reads;
    if(type==&data_type) return &data;
    Check(type==&event_type); return current;
}
void __fastcall Immediate(void* p,bool value,void*) { Check(p==own && value); }
void __fastcall Die(void* w,void* p,void* source,void*) {
    Check(w==world && p==own && !source && current->records.empty()); ++retirements;
    api::TrackedEntities().Disposed(api::TrackedEntities().Current(own));
}
void* __fastcall ContextOf(Component* value,void*) { ++reads; return value->context; }
void* __fastcall Caster(Context* value,void*) { ++reads; return value->caster; }
void* __fastcall Records(Component* value,void*) { ++reads; return &value->records; }
int __fastcall Count(Values* value,void*) { return static_cast<int>(value->size()); }
void* __fastcall Item(Values* value,int index,void*) { return value->at(index); }
bool __fastcall Equal(std::wstring* a,std::wstring* b,void*) { return *a==*b; }
void* __fastcall Register(Component* value,Context* ctx,void* path,void*) {
    Check(value==current && ctx==value->context && ctx->caster==own && path!=&shared); ++registrations;
    record_storage.push_back(std::make_unique<int>(static_cast<int>(registrations)));
    auto* record=record_storage.back().get(); value->records.push_back(record);
    if(dispose_on_register) api::TrackedEntities().Disposed(api::TrackedEntities().Current(own));
    return record;
}
void __fastcall Detach(Component* value,void* record,bool option,void*) {
    Check(option); ++detaches;
    value->records.erase(std::remove(value->records.begin(),value->records.end(),record),value->records.end());
}
void __fastcall Dispose(void*,void*) { ++disposals; }
struct Types:api::ManagedObjects {
    api::ObjectResult ManagedType(const char* ns,const char* name) override {
        if(std::string(name)=="AdventureCharacterDataComponent") { Check(std::string(ns)=="RPG.GameCore"); return Ref(&data_type); }
        Check(std::string(ns).empty() && std::string(name)=="LGBHONLHNPN"); return Ref(&event_type);
    }
    api::ObjectResult AllocateObject(const char*,const char*) override { throw std::runtime_error("no allocation needed"); }
    api::ObjectResult NewStringUtf8(const char*) override { throw std::runtime_error("no array/string recreation"); }
};
void Reset() {
    Check(handles.empty() && !api::TrackedEntities().Current(own).entity);
    component.consumer=&consumer; component.context=&context; component.records.clear();
    other_component.consumer=&next_consumer; other_component.context=&other_context; other_component.records.clear();
    current=&component; config.events=&baseline; loaded=true; dispose_on_register=false;
    reads=registrations=detaches=disposals=retirements=0; record_storage.clear();
}
}
void TestNpcEventSupplement() {
    const api::ReferenceMethods refs{M(Keep),M(Target),M(Free),{}, {}};
    const api::EntityAccessMethods entities{M(Id),M(World),M(Loaded),M(ComponentOf),{},M(Immediate),M(Die),{}};
    std::vector<api::DeclaredField> fields{{"RPG.GameCore","AdventureCharacterDataComponent","config",
        "RPG.GameCore.AdventureCharacterConfig",offsetof(Data,config),6,true}};
    // Names/relationship are from the old runtime consumer; fields and offsets
    // are synthetic. No claim that this is a retained game field declaration.
    const auto data_field=api::ResolveAdventureDataField(fields); Check(data_field.available);
    auto static_field=fields.front(); static_field.flags=0x16; fields.push_back(static_field);
    Check(api::ResolveAdventureDataField(fields).available);
    fields.push_back(fields.front()); Check(!api::ResolveAdventureDataField(fields).available);
    fields={fields.front()}; fields.front().flags_observed=false;
    Check(!api::ResolveAdventureDataField(fields).available);
    Check(api::ReadAdventureDataConfig(&data,data_field).object==&config);
    api::AdventureConfigMethods config_methods; config_methods.has_event_paths=true; config_methods.event_paths_offset=offsetof(Config,events);
    api::AnimationEventMethods methods{M(ContextOf),M(Caster),M(Records),M(Register),M(Detach),M(Dispose),{}};
    methods.has_consumer=true; methods.consumer_offset=offsetof(Component,consumer);
    Types types;
    const auto make_events=[&]{
        api::PresentationBindings bindings;bindings.references=refs;bindings.entity=entities;bindings.adventure_data=data_field;
        bindings.config=config_methods;bindings.events=methods;bindings.arrays={{},M(Count),M(Item),{}, {}};
        bindings.event_list={M(Count),M(Item),{}};bindings.scene={{},{},{},M(Equal),{}};
        api::PresentationContext sdk_context(types,bindings);return sdk_context.NpcEvents();
    }();
    const auto component_types=api::ResolveAnimationInputTypes(types);
    api::NpcCreateInput input{world,reinterpret_cast<api::EntityManager*>(&manager),
        reinterpret_cast<api::MapNpcDefinition*>(&definition),reinterpret_cast<api::NpcDataRow*>(&row),true};
    api::WorldFrame frame; frame.world=world; frame.disposing={false,{true,0}};
    auto foreign=frame; foreign.world=reinterpret_cast<api::GameWorld*>(1);
    Reset();
    {
        api::NpcCreationSession npc(api::EntityOwner{},M(Create),entities,refs); npc.Create(input);
        auto owned=make_events(npc,world);auto& events=*owned; events.Prepare(component_types,Ref(&desired));
        const auto before=reads; events.Service(foreign); Check(reads==before && registrations==0);
        loaded=false; events.Service(frame); Check(registrations==0);
        loaded=true; events.Service(frame); events.Service(frame);
        Check(registrations==2 && events.Status().incarnations==1 && component.records.size()==2);
        events.SetEnabled(false); events.Service(frame);
        Check(events.Quiescent() && detaches==2 && disposals==2 && component.records.empty());
        events.SetEnabled(true); events.Service(frame); Check(registrations==4 && events.Status().incarnations==2);
        // New component on the same live NPC: no call back into the old one.
        current=&other_component; events.Service(frame);
        Check(registrations==6 && detaches==2 && events.Status().incomplete_retirements==1);
        // New baseline on same consumer: retire old supplement, then recompute
        // the difference. No duplicate baseline registration or player lookup.
        config.events=&other_baseline; events.Service(frame);
        Check(registrations==7 && detaches==4 && current->records.size()==1);
        npc.Remove(); events.Service(frame); Check(events.Status().closed && current->records.empty());
        npc.Service(frame); Check(npc.Status().closed && retirements==1 && handles.empty());
    }
    Reset();
    {
        api::NpcCreationSession npc(api::EntityOwner{},M(Create),entities,refs); npc.Create(input);
        auto owned=make_events(npc,world);auto& events=*owned; events.Prepare(component_types,Ref(&desired)); events.Service(frame);
        api::TrackedEntities().Disposed(api::TrackedEntities().Current(own));
        const auto before=reads;
        events.Service(foreign); npc.Service(foreign);
        Check(events.Status().closed && npc.Status().closed && reads==before && detaches==0 && handles.empty());
    }
    Reset();
    {
        api::NpcCreationSession npc(api::EntityOwner{},M(Create),entities,refs); npc.Create(input);
        auto owned=make_events(npc,world);auto& events=*owned; events.Prepare(component_types,Ref(&desired));
        dispose_on_register=true; events.Service(frame);
        Check(registrations==1 && events.Active()->Status().owner_ended);
        const auto before=reads; events.Service(foreign); npc.Service(foreign);
        Check(reads==before && events.Status().closed && handles.empty());
    }
    Reset();
    {
        api::NpcCreationSession npc(api::EntityOwner{},M(Create),entities,refs); npc.Create(input);
        auto owned=make_events(npc,world);auto& events=*owned; events.Prepare(component_types,Ref(&desired)); events.Service(frame);
        Check(!events.InputsDetached());
        events.SetEnabled(false); free_fault=true; events.Service(frame);
        Check(events.InputsDetached() && !events.Quiescent() && component.records.empty());
        events.SetEnabled(true); events.Service(frame);
        Check(registrations==4 && component.records.size()==2); // unresolved old GC is not a registration gate
        npc.Remove(); events.Service(frame); npc.Service(frame);
        Check(events.InputsDetached() && !events.Status().closed && npc.Status().closed && handles.empty());
        // The native GC test function removed its handle before raising. The
        // production receipt correctly remains unresolved and is not retried.
    }
    Reset();
    {
        struct Strings:Types {
            std::vector<std::unique_ptr<std::string>> values;
            api::ObjectResult NewStringUtf8(const char* value)override {values.push_back(std::make_unique<std::string>(value));return Ref(values.back().get());}
        } strings;
        api::EntityCreationSession actor(api::EntityOwner{},{},M(Story),&strings,entities,refs);
        // Construct the supplement before Create, as the real framework does.
        api::NpcEventSupplement events(actor,world,refs,
            {api::EntityAccess(entities),data_field,api::AdventureConfig(config_methods),api::AnimationEvents(methods),
             api::ManagedArrays({{},M(Count),M(Item),{}, {}}),api::ManagedReferenceList({M(Count),M(Item),{}}),
             api::SceneObjects({{},{},{},M(Equal),{}})},offsetof(Config,performance));
        actor.Create(api::StoryCreateInput{world,reinterpret_cast<api::EntityManager*>(&manager),"story","model","config"});
        config.events=&desired; // Ordinary list is desired, NOT the actual factory baseline.
        config.performance=&baseline;
        events.Prepare(component_types,Ref(&desired));events.Service(frame);
        Check(registrations==2 && events.Status().incarnations==1);
        actor.Remove();events.Service(frame);actor.Service(frame);
        Check(events.Status().closed && actor.Status().closed && handles.empty());
    }
    Reset();
    std::cout<<"NPC/story event integration passed: distinct factory baselines, source delta, domain pause/resume, replacement, retirement and natural exit; no game acceptance.\n";
}
