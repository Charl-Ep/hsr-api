#include <hksr/presentation/effects/EffectAccess.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#include <Windows.h>
#include <stdexcept>
#include <iostream>

namespace {
using namespace hksr::api;
int path,mapped,entity,manager,instance,info;
struct Params {std::uint32_t id=0;bool released=false;int state=0;} input;
int submitted=0,removed=0,released=0;bool fault=false;
bool empty_path=false;
bool __fastcall Empty(void*,void*){return empty_path;}
void Check(bool value){if(!value)throw std::runtime_error("effect access test failed");}
template<class T>Method M(T entry){return {reinterpret_cast<void*>(entry),&info};}
void* __fastcall Map(void* owner,void* name,void* meta){Check(owner==&entity && name==&path && meta==&info);return &mapped;}
EffectReference __fastcall Acquire(void* name,void* meta){Check(name==&mapped && meta==&info);return {input.id,0,&input};}
unsigned __fastcall Id(void* object,void* meta){Check(object==&input && meta==&info);return input.id;}
bool __fastcall Released(void* object,void*){Check(object==&input);return input.released;}
int __fastcall State(void* object,void*){Check(object==&input);return input.state;}
void* __fastcall Instance(void* object,void*){Check(object==&input);return input.state==3?&instance:nullptr;}
void __fastcall Submit(void* service,void* object,void* meta) {
    Check(service==&manager && object==&input && meta==&info);++submitted;input.state=1;
    if(fault)RaiseException(0xE0670001,0,0,nullptr);
}
bool __fastcall Remove(void* service,EffectReference value,bool fade,void* meta) {
    Check(service==&manager && value.parameters==&input && fade && meta==&info);++removed;
    if(value.id!=input.id)return false;
    input.state=6;return true; // accepted cancellation, intentionally not yet released
}
void __fastcall Release(void* object,void* meta){Check(object==&input && meta==&info);++released;input.released=true;}
}
void TestEffectAccess() {
    DeclaredType extensions{"RPG.GameCore","GameEntityExtensions",{
        {"MapResPath","System.Boolean",{"RPG.GameCore.GameEntity","System.String&"},true,{}},
        {"MapResPath","System.String",{"RPG.GameCore.GameEntity","System.String"},true,M(Map)}}};
    DeclaredType service{"RPG.Client","MonoEffectManager",{
        {"GetTriggerEffectParamsRef","RPG.Client.TriggerEffectParamsRef",{"System.String"},true,M(Acquire)},
        {"FireEffectAsync","System.Void",{"RPG.Client.TriggerEffectParams"},false,M(Submit)},
        {"RemoveEffectByTriggerParamsRef","System.Boolean",{"RPG.Client.TriggerEffectParamsRef","System.Boolean"},false,M(Remove)},
        {"ReleaseTriggerEffectParam","System.Void",{"RPG.Client.TriggerEffectParams"},true,M(Release)}}};
    DeclaredType params{"RPG.Client","TriggerEffectParams",{
        {"get_UniqueEffectID","System.UInt32",{},false,M(Id)},
        {"get_IsReleased","System.Boolean",{},false,M(Released)},
        {"get_AliveState","RPG.Client.MonoEffectAliveState",{},false,M(State)},
        {"get_EffectInstance","RPG.Client.MonoEffect",{},false,M(Instance)}}};
    DeclaredType strings{"System","String",{{"IsNullOrEmpty","System.Boolean",{"System.String"},true,M(Empty)}}};
    MetadataResolver metadata({extensions,service,params,strings});const auto methods=ResolveEffectAccess460(metadata);
    Check(methods.binding_errors.empty());EffectAccess api(methods);
    Check(api.MapPath(&entity,&path).object==&mapped);
    // Observation can describe zero, but removal must not forward that value:
    // the official removal filter treats it as an unrestricted ID.
    auto request=api.Acquire(&mapped);Check(request.call.returned && request.reference.id==0 && request.reference.parameters==&input);
    Check(api.Observe(request.reference).same_generation && !api.Observe(request.reference).released.value);
    Check(!api.Remove(&manager,request.reference,true).read.returned && removed==0);
    input.id=1;request=api.Acquire(&mapped);
    Check(api.Submit(&manager,request.reference).returned && submitted==1);
    Check(api.Remove(&manager,request.reference,true).value && removed==1 && released==0 && !input.released);
    // Request cancellation did not submit an extra Release call.
    input.id=0xf0000011;input.state=0;
    Check(!api.Observe(request.reference).same_generation);
    Check(!api.Submit(&manager,request.reference).returned && submitted==1);
    Check(!api.ReleaseUnsubmitted(request.reference).returned && released==0);
    Check(!api.Remove(&manager,request.reference,true).value && removed==2);
    request=api.Acquire(&mapped);Check(request.reference.id==0xf0000011);
    Check(api.ReleaseUnsubmitted(request.reference).returned && released==1);
    Check(api.ReleaseUnsubmitted(request.reference).returned && released==1);
    Check(!api.Submit(&manager,request.reference).returned && submitted==1);
    ++input.id;input.released=false;request=api.Acquire(&mapped);fault=true;
    const auto failed=api.Submit(&manager,request.reference);fault=false;
    Check(!failed.returned && failed.exception==0xE0670001 && submitted==2 && released==1);
    Check(api.Remove(&manager,request.reference,true).value && released==1); // preserve partial handoff, no blind pooling
    input.state=3;Check(api.Observe(request.reference).instance.object==&instance);
    Check(!api.MapPath(nullptr,&path).call.returned && !api.Acquire(nullptr).call.returned);
    empty_path=true;Check(api.Acquire(&mapped).call.returned && !api.Acquire(&mapped).reference.parameters);empty_path=false;
    params.methods.pop_back();MetadataResolver partial({extensions,service,params,strings});
    const auto subset=ResolveEffectAccess460(partial);Check(subset.binding_errors.size()==1);
    Check(EffectAccess(subset).Observe(request.reference).same_generation); // optional instance query failure is separate
    std::cout<<"Effect access: exact overloads, 16-byte return/argument ABI, pool generations, partial submission and non-owning cancellation passed; no game effects.\n";
}
