#include <hksr/presentation/effects/EffectActions.hpp>
#include <Windows.h>
#include <functional>
#include <map>
#include <stdexcept>
#include <iostream>

namespace {
using namespace hksr::api;
struct Params {unsigned id=9;bool released=false;void* effect=nullptr;} request,other{10};
struct Effect {void* request;};Effect first{&request},second{&other};
int info,manager,type,text,array,animator0,animator1,filter,serial=0,queries=0,sets=0,resets=0,removes=0,releases=0;
std::map<int,void*> roots;std::vector<void*> children;std::function<void()> on_query,on_trigger,on_acquire;
bool trigger_fault=false,remove_fault=false,release_fault=false;unsigned removed_id=0;EffectRemovalOptions actual;
void CheckAt(bool v,int line){if(!v)throw std::runtime_error("effect actions failed at "+std::to_string(line));}
#define Check(v) CheckAt((v),__LINE__)
template<class T>Method M(T f){return {reinterpret_cast<void*>(f),&info};}
bool Kept(void* p){for(const auto& r:roots)if(r.second==p)return true;return false;}
int __fastcall Keep(void* p,int,int,void*){roots[++serial]=p;return serial;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){roots.erase(id);}
unsigned __fastcall Id(Params* p,void*){return p->id;}
bool __fastcall Released(Params* p,void*){return p->released;}
void* __fastcall Instance(Params* p,void*){return p->effect;}
bool __fastcall Alive(void* p,void*){Check(Kept(p));return true;}
void* __fastcall Children(void* p,void* t,bool inactive,void* meta){
    Check(p==&first && t==&type && inactive && Kept(p) && Kept(t) && meta==&info);++queries;
    if(on_query)on_query();return &array;
}
int __fastcall Length(void* p,void*){Check(p==&array && Kept(p));return static_cast<int>(children.size());}
void* __fastcall Item(void* p,int index,void*){Check(p==&array && Kept(p));return children.at(index);}
void Trigger(void* p,void* name,bool reset,void* meta){
    Check((p==&animator0 || p==&animator1) && Kept(p) && name==&text && Kept(name) && meta==&info);
    if(reset)++resets;else ++sets;if(on_trigger)on_trigger();
    if(trigger_fault)RaiseException(0xE0700001,0,0,nullptr);
}
void __fastcall Set(void* p,void* n,void* m){Trigger(p,n,false,m);}
void __fastcall Reset(void* p,void* n,void* m){Trigger(p,n,true,m);}
void* __fastcall Acquire(void* entity,void* path,void* unique,void* point,bool unbind,bool immediate,unsigned id,void* flags,bool include,void* meta){
    Check(!entity && !path && !unique && !point && !flags && include && id && meta==&info);
    Check(Kept(&manager) && Kept(&request));actual.unbind=unbind;actual.immediate_fade=immediate;removed_id=id;
    if(on_acquire)on_acquire();return &filter;
}
bool __fastcall Remove(void* m,void* f,bool fade,void* meta){
    Check(m==&manager && f==&filter && Kept(m) && Kept(f) && meta==&info);actual.fade=fade;++removes;
    if(remove_fault)RaiseException(0xE0700002,0,0,nullptr);return true;
}
void __fastcall Release(void* f,void*){Check(f==&filter);++releases;if(release_fault)RaiseException(0xE0700003,0,0,nullptr);}
struct Objects:ManagedObjects {
    ObjectResult NewStringUtf8(const char* name)override{Check(std::string(name)=="Fire");return {{true,0},&text};}
    ObjectResult AllocateObject(const char*,const char*)override{throw std::runtime_error("no task allocation");}
    ObjectResult ManagedType(const char* ns,const char* name)override{
        Check(std::string(ns)=="UnityEngine" && std::string(name)=="Animator");return {{true,0},&type};
    }
};
}
void TestEffectActions(){
    MetadataResolver metadata({
        {"UnityEngine","Component",{{"GetComponentsInChildren","UnityEngine.Component[]",{"System.Type","System.Boolean"},false,M(Children)}}},
        {"UnityEngine","Object",{{"op_Implicit","System.Boolean",{"UnityEngine.Object"},true,M(Alive)}}},
        {"UnityEngine","Animator",{{"SetTrigger","System.Void",{"System.String"},false,M(Set)},
            {"ResetTrigger","System.Void",{"System.String"},false,M(Reset)}}},
        {"RPG.Client","MonoEffectManager",{
            {"GetRemoveEffectParam","PCHFCKDEAPK",{"RPG.GameCore.GameEntity","System.String","System.String","System.String","System.Boolean","System.Boolean","System.UInt32","RPG.GameCore.MonoEffectFlag[]","System.Boolean"},true,M(Acquire)},
            {"RemoveEffect","System.Boolean",{"PCHFCKDEAPK","System.Boolean"},false,M(Remove)},
            {"ReleaseRemoveEffectParam","System.Void",{"PCHFCKDEAPK"},true,M(Release)}}}});
    auto methods=ResolveEffectActions460(metadata);Check(methods.binding_errors.empty());
    EffectAccessMethods access;access.id=M(Id);access.released=M(Released);access.instance=M(Instance);
    ReferenceMethods refs{M(Keep),M(Target),M(Free),{},{}};ManagedArrays arrays({{},M(Length),M(Item),{},{}});Objects objects;
    const std::vector<DeclaredField> fields{{"RPG.Client","MonoEffect","TriggerParams","RPG.Client.TriggerEffectParams",offsetof(Effect,request),6,true}};
    EffectActions actions(EffectAccess(access),methods,refs,objects,arrays,fields);
    const EffectReference ref{9,0,&request};request.effect=&first;other.effect=&second;children={&animator0,&animator1};
    Check(actions.Trigger(ref,"Fire").state==EffectControlState::Applied && sets==2 && roots.size()==1);
    Check(actions.Trigger(ref,"Fire",true).state==EffectControlState::Applied && resets==2);
    request.effect=nullptr;Check(actions.Trigger(ref,"Fire").state==EffectControlState::NotLoaded);request.effect=&first;
    on_query=[]{++request.id;};Check(actions.Trigger(ref,"Fire").state==EffectControlState::Ended && sets==2);on_query={};request.id=9;
    on_query=[]{first.request=&other;};Check(actions.Trigger(ref,"Fire").state==EffectControlState::Failed && sets==2);on_query={};first.request=&request;
    on_trigger=[]{request.effect=&second;};Check(actions.Trigger(ref,"Fire").state==EffectControlState::Failed && sets==3);on_trigger={};request.effect=&first;
    trigger_fault=true;auto failed=actions.Trigger(ref,"Fire");trigger_fault=false;
    Check(failed.state==EffectControlState::Failed && failed.call.exception==0xE0700001 && sets==4 && roots.size()==1);
    children.clear();Check(actions.Trigger(ref,"Fire").state==EffectControlState::Applied && sets==4);
    // Removal works while loading. The exact ID, both fade flags and unbind
    // reach the factory/manager; no path or target-wide selector is supplied.
    request.effect=nullptr;const auto removed=actions.Remove(&manager,ref,{true,true,true});
    Check(removed.removal.read.returned && removed.removal.value && removed.input_release.returned);
    Check(actual.fade && actual.immediate_fade && actual.unbind && removed_id==9 && removes==1 && releases==1);
    Check(!request.released && !other.released && roots.size()==1);
    request.id=0;Check(!actions.Remove(&manager,{0,0,&request},{}).removal.read.returned && removes==1);request.id=9;
    on_acquire=[]{++request.id;};Check(!actions.Remove(&manager,ref,{}).removal.read.returned && removes==1 && releases==2);on_acquire={};request.id=9;
    remove_fault=true;const auto error=actions.Remove(&manager,ref,{});remove_fault=false;
    Check(error.removal.read.exception==0xE0700002 && error.input_release.returned && releases==3);
    release_fault=true;Check(!actions.Remove(&manager,ref,{}).input_release.returned);release_fault=false;
    Check(actions.Close() && roots.empty());Check(!actions.Remove(&manager,ref,{}).removal.read.returned);
    std::cout<<"Effect actions: exact trigger wrappers, recursive inactive children, pool identity, explicit fade/removal inputs and partial failures passed; no game playback.\n";
}
