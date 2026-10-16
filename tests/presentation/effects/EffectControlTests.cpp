#include <hksr/presentation/effects/EffectControl.hpp>
#include <Windows.h>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
using namespace hksr::api;
struct Params {unsigned id=0;bool released=false;void* effect=nullptr;};
struct Effect {void* request=nullptr;int animator=0,plugin=0,behavior=0;} first,second;
Params a{0,false,&first},b{7,false,&second};
int info,animator_type,plugin_type,serial=0,writes=0,queries=0,types=0;
std::map<int,void*> roots;
std::function<void()> on_component;
void* mutated=nullptr;float actual_value=0,actual_duration=0,actual_start=0;
bool absent=false,fault=false,dead=false,id_error=false,foreign_owner=false;
void Check(bool ok){if(!ok)throw std::runtime_error("effect control test failed");}
template<class T>Method M(T entry){return {reinterpret_cast<void*>(entry),&info};}
bool Kept(void* object){for(const auto& r:roots)if(r.second==object)return true;return false;}
int __fastcall Keep(void* object,int,int,void*){roots[++serial]=object;return serial;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){Check(roots.erase(id)==1);}
unsigned __fastcall Id(void* object,void*){if(id_error)RaiseException(0xE0670001,0,0,nullptr);return static_cast<Params*>(object)->id;}
bool __fastcall Released(void* object,void*){return static_cast<Params*>(object)->released;}
void* __fastcall Instance(void* object,void*){return static_cast<Params*>(object)->effect;}
bool __fastcall Alive(void*,void*){return !dead;}
void* __fastcall Component(void* object,void* type,void* meta) {
    Check(Kept(object) && Kept(type) && meta==&info);++queries;
    if(on_component)on_component();
    if(absent)return nullptr;
    auto* effect=static_cast<Effect*>(object);
    return type==&animator_type?&effect->animator:&effect->plugin;
}
void* __fastcall Behavior(void* plugin,void*) {
    Check(Kept(plugin));return plugin==&first.plugin?&first.behavior:&second.behavior;
}
void* __fastcall Owner(void* behavior,void*) {
    Check(Kept(behavior));if(foreign_owner)return &second.plugin;
    return behavior==&first.behavior?&first.plugin:&second.plugin;
}
void __fastcall Progress(void* receiver,float value,float seconds,void* meta) {
    Check(Kept(receiver) && meta==&info);mutated=receiver;actual_value=value;actual_duration=seconds;++writes;
    if(fault)RaiseException(0xE0670001,0,0,nullptr);
}
void __fastcall Play(void* receiver,int hash,int layer,float start,void* meta) {
    Check(Kept(receiver) && hash==123 && layer==0 && meta==&info);mutated=receiver;actual_start=start;++writes;
}
void __fastcall CrossFade(void* receiver,int hash,float duration,int layer,float start,float transition,void* meta) {
    Check(transition==0);actual_duration=duration;Play(receiver,hash,layer,start,meta);
}
struct Objects:ManagedObjects {
    ObjectResult NewStringUtf8(const char*)override{throw std::runtime_error("no strings needed");}
    ObjectResult AllocateObject(const char*,const char*)override{throw std::runtime_error("no task allocation allowed");}
    ObjectResult ManagedType(const char* ns,const char* name)override {
        ++types;const bool animator=std::string(name)=="Animator";
        Check(std::string(ns)==(animator?"UnityEngine":"RPG.Client"));
        Check(animator || std::string(name)=="MonoEffectPluginProgress");
        return {{true,0},animator?&animator_type:&plugin_type};
    }
};
}
void TestEffectControl() {
    first.request=&a;second.request=&b;
    MetadataResolver metadata({
        {"UnityEngine","Component",{{"GetComponent","UnityEngine.Component",{"System.Type"},false,M(Component)},
            {"GetComponent","UnityEngine.Component",{"System.String"},false,{}}}},
        {"UnityEngine","Object",{{"op_Implicit","System.Boolean",{"UnityEngine.Object"},true,M(Alive)}}},
        {"RPG.Client","MonoEffectPluginProgress",{{"get_Behavior","LMGBJIFBAAN",{},false,M(Behavior)}}},
        {"","LMGBJIFBAAN",{{"HPNDIIPEFLJ","RPG.Client.MonoEffectPluginProgress",{},false,M(Owner)},
            {"EJDAKAECODE","System.Void",{"System.Single","System.Single"},false,M(Progress)}}},
        {"UnityEngine","Animator",{{"Play","System.Void",{"System.Int32","System.Int32","System.Single"},false,M(Play)},
            {"CrossFade","System.Void",{"System.Int32","System.Single","System.Int32","System.Single","System.Single"},false,M(CrossFade)}}}});
    auto methods=ResolveEffectControl460(metadata);Check(methods.binding_errors.empty());
    EffectAccessMethods access;access.id=M(Id);access.released=M(Released);access.instance=M(Instance);
    // State/manager/load methods intentionally absent. They are not needed to
    // control one already loaded request and must not become unrelated gates.
    Objects objects;const ReferenceMethods refs{M(Keep),M(Target),M(Free),{}, {}};
    const std::vector<DeclaredField> fields{{"RPG.Client","MonoEffect","TriggerParams","RPG.Client.TriggerEffectParams",offsetof(Effect,request),6,true}};
    EffectControl control(EffectAccess(access),methods,refs,objects,fields);
    const EffectReference ra{0,0,&a},rb{7,0,&b};
    Check(control.SetProgress(ra,0.8f,0.183f).state==EffectControlState::Applied && mutated==&first.behavior);
    Check(actual_value==0.8f && actual_duration==0.183f);
    Check(control.SetProgress(rb,1,0.1f).state==EffectControlState::Applied && mutated==&second.behavior);
    Check(control.SetAnimatorState(ra,123).state==EffectControlState::Applied && mutated==&first.animator && actual_start==0);
    Check(control.SetAnimatorState(ra,123,0.2f,false).state==EffectControlState::Applied &&
        std::isinf(actual_start) && actual_start<0 && actual_duration==0.2f && types==2);
    const auto before=writes;
    a.effect=nullptr;Check(control.SetProgress(ra,1).state==EffectControlState::NotLoaded && writes==before);a.effect=&first;
    a.released=true;Check(control.SetProgress(ra,1).state==EffectControlState::Ended);a.released=false;
    ++a.id;Check(control.SetProgress(ra,1).state==EffectControlState::Ended);a.id=0;
    first.request=&b;Check(control.SetProgress(ra,1).state==EffectControlState::Failed);first.request=&a;
    absent=true;Check(control.SetProgress(ra,1).state==EffectControlState::Unsupported);absent=false;
    id_error=true;Check(control.SetProgress(ra,1).state==EffectControlState::Failed);id_error=false;
    dead=true;Check(control.SetProgress(ra,1).state==EffectControlState::Ended);dead=false;
    foreign_owner=true;Check(control.SetProgress(ra,1).state==EffectControlState::Failed);foreign_owner=false;
    on_component=[] {++a.id;};Check(control.SetProgress(ra,1).state==EffectControlState::Ended);on_component={};a.id=0;
    on_component=[] {a.effect=&second;};Check(control.SetProgress(ra,1).state==EffectControlState::Failed);on_component={};a.effect=&first;
    Check(writes==before && roots.size()==2); // only the cached component types
    fault=true;const auto failed=control.SetProgress(ra,0.4f);fault=false;
    Check(failed.state==EffectControlState::Failed && failed.call.exception==0xE0670001 && failed.references_released && writes==before+1);
    // A missing progress method does not disable an independent Animator call.
    methods.progress_set={};EffectControl partial(EffectAccess(access),methods,refs,objects,fields);
    Check(partial.SetProgress(ra,1).state==EffectControlState::Unsupported);
    Check(partial.SetAnimatorState(ra,123).state==EffectControlState::Applied);
    Check(partial.Close());const auto old_queries=queries;
    Check(control.Close() && roots.empty());Check(control.SetProgress(ra,1).state==EffectControlState::Failed && queries==old_queries);
    std::cout<<"Effect control: request isolation, deferred loads, pool reuse, component ownership, native arguments and partial failure passed; no game playback.\n";
}
