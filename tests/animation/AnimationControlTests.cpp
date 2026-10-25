#include <hksr/animation/AnimationControl.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#include <Windows.h>
#include <iostream>
#include <stdexcept>
#include <cstring>

namespace {
using namespace hksr::api;
int animator,style,model,loader,manager,controller,name,path,tag,info;
int parameter;float parameter_value=0.7f;int parameter_kind=1;
struct Motion {void* header[2]{};float clock=.75f;} motion;
float flight_height=0,fall_height=.6f;int flight_reads=0,fall_reads=0;
struct BlinkMask {bool active[4]{},value[4]{};} blink_mask;
struct Emotion {void* header[2]{};BlinkMask* mask=&blink_mask;} emotion;
unsigned played=0,npc_played=0,state_played=0,path_calls=0,object_calls=0;
bool fault=false;
void Check(bool value) { if(!value) throw std::runtime_error("animation control call mismatch"); }
template<class T> Method M(T function) { return {reinterpret_cast<void*>(function),&info}; }
int __fastcall Hash(void* text,void* meta) { Check(text==&name && meta==&info); return -731; }
bool __fastcall Has(void* object,int layer,int hash,void* meta) {
    Check(object==&animator && layer==2 && meta==&info); return hash==-731;
}
void* __fastcall Controller(void* object,void* meta) { Check(object==&animator && meta==&info); return &controller; }
void* __fastcall Resolve(void* object,int hash,void* meta) {Check(object==&animator && hash==-731 && meta==&info);return &name;}
void* __fastcall LayerName(void* object,int layer,void* meta){Check(object==&animator && layer==0 && meta==&info);return &name;}
float __fastcall FlightHeight(void* object,float clock,void* meta){Check(object==&motion && clock==motion.clock && meta==&info);++flight_reads;return flight_height;}
float __fastcall FallHeight(void* object,void* meta){Check(object==&motion && meta==&info);++fall_reads;return fall_height;}
void __fastcall Play(void* object,int hash,int layer,float time,void* meta) {
    Check(object==&animator && hash==-731 && layer==2 && time==0.25f && meta==&info); ++played;
    if(fault) RaiseException(0xE0670001,0,0,nullptr);
}
void __fastcall Npc(void* object,unsigned id,void* meta) { Check(object==&style && id==0xF0000011u && meta==&info); ++npc_played; }
void __fastcall State(void* object,int hash,float first,float second,void* meta) {
    Check(object==&style && hash==-731 && first==0.125f && second==0.75f && meta==&info); ++state_played;
}
void __fastcall Path(void* object,void* value,void* key,void* meta) {
    Check(object==&model && (value==&path || !value) && !key && meta==&info); ++path_calls;
}
void* __fastcall Manager(void* object,void* meta) { Check(object==&loader && meta==&info); return &manager; }
void __fastcall Object(void* object,void* value,void* key,void* meta) {
    Check(object==&manager && (value==&controller || !value) && key==&tag && meta==&info); ++object_calls;
}
void __fastcall StateInfo(void* object,int layer,int index,unsigned char* output,void* meta) {
    Check(object==&animator && layer==0 && (index==0 || index==1) && meta==&info);
    std::memset(output,0,36); const int hash=-731; const float time=0.375f;
    std::memcpy(output,&hash,4); std::memcpy(output+12,&time,4);
}
bool __fastcall InTransition(void* object,int layer,void*) {Check(object==&animator && layer==0);return false;}
int __fastcall LayerCount(void* object,void* meta) {Check(object==&animator && meta==&info);return 5;}
int __fastcall LayerIndex(void* object,void* text,void* meta){Check(object==&animator && text==&name && meta==&info);return 3;}
float __fastcall LayerWeight(void* object,int layer,void* meta){Check(object==&animator && layer==3 && meta==&info);return 0.75f;}
void __fastcall SetLayerWeight(void* object,int layer,float weight,void* meta){Check(object==&animator && layer==3 && weight==0 && meta==&info);}
void __fastcall Update(void* object,float delta,void* meta){Check(object==&animator && delta==0 && meta==&info);}
void __fastcall Fade(void* object,int hash,float duration,int layer,float time,float progress,void* meta) {
    Check(object==&animator && hash==-731 && duration==0.15f && layer==2 && time==0 && progress==0 && meta==&info);
}
bool __fastcall BlinkActive(void* object,int source,void* meta){Check(object==&blink_mask && source==3 && meta==&info);return blink_mask.active[source];}
bool __fastcall BlinkValue(void* object,int source,void* meta){Check(object==&blink_mask && source==3 && meta==&info);return blink_mask.value[source];}
void __fastcall BlinkSet(void* object,int source,bool value,void* meta) {
    Check(object==&emotion && source==3 && meta==&info);blink_mask.active[source]=true;blink_mask.value[source]=value;
}
int __fastcall ReadyHash(std::int16_t id,void* meta){Check(id==2 && meta==&info);return 712;}
int __fastcall ParameterCount(void* object,void*){Check(object==&animator);return 1;}
void* __fastcall Parameter(void* object,int index,void*){Check(object==&animator && index==0);return &parameter;}
int __fastcall ParameterHash(void* object,void*){Check(object==&parameter);return 712;}
int __fastcall ParameterType(void* object,void*){Check(object==&parameter);return parameter_kind;}
float __fastcall GetFloat(void* object,int hash,void*){Check(object==&animator && hash==712);return parameter_value;}
void __fastcall SetFloat(void* object,int hash,float value,void*){Check(object==&animator && hash==712);parameter_value=value;}
}
void TestAnimationControl() {
    struct ModelLinks { void* header[2]; void* loader; } links{{nullptr,nullptr},&loader};
    std::vector<DeclaredField> fields{{"RPG.GameCore","CharacterModelComponent","_AnimOverrideCmpt","NLMPFKPGCNA",offsetof(ModelLinks,loader),0,true}};
    auto link=ResolveModelControllerLoader(fields);
    Check(link.available && ReadControllerLink(&links,link).object==&loader);
    fields.front().flags=0x10; Check(!ResolveModelControllerLoader(fields).available);
    fields.front().flags=0; fields.front().flags_observed=false; Check(!ResolveModelControllerLoader(fields).available);
    fields.front().flags_observed=true; fields.push_back(fields.front()); Check(!ResolveModelControllerLoader(fields).available);
    Check(ResolveControllerLoaderModel({{"","NLMPFKPGCNA","EHFENKGDGJD","RPG.GameCore.CharacterModelComponent",64,0,true}}).available);
    Check(ResolveControllerLoaderManager({{"","NLMPFKPGCNA","EOJOJPEPCPN","RPG.Client.AnimatorOverrideManager",48,0,true}}).available);
    // Signatures transcribed from retained game declarations; only function
    // bodies are substitutes. Wrong overloads are present to test selection.
    DeclaredType anim{"UnityEngine","Animator",{
        {"StringToHash","System.Int32",{"System.String"},true,M(Hash)},
        {"ResolveHash","System.String",{"System.Int32"},false,M(Resolve)},
        {"GetLayerName","System.String",{"System.Int32"},false,M(LayerName)},
        {"HasState","System.Boolean",{"System.Int32","System.Int32"},false,M(Has)},
        {"get_runtimeAnimatorController","UnityEngine.RuntimeAnimatorController",{},false,M(Controller)},
        {"GetAnimatorStateInfo","System.Void",{"System.Int32","UnityEngine.StateInfoIndex","UnityEngine.AnimatorStateInfo&"},false,M(StateInfo)},
        {"IsInTransition","System.Boolean",{"System.Int32"},false,M(InTransition)},
        {"get_layerCount","System.Int32",{},false,M(LayerCount)},
        {"GetLayerIndex","System.Int32",{"System.String"},false,M(LayerIndex)},
        {"GetLayerWeight","System.Single",{"System.Int32"},false,M(LayerWeight)},
        {"SetLayerWeight","System.Void",{"System.Int32","System.Single"},false,M(SetLayerWeight)},
        {"Update","System.Void",{"System.Single"},false,M(Update)},
        {"GetFloat","System.Single",{"System.Int32"},false,M(GetFloat)},
        {"SetFloat","System.Void",{"System.Int32","System.Single"},false,M(SetFloat)},
        {"get_parameterCount","System.Int32",{},false,M(ParameterCount)},
        {"GetParameter","UnityEngine.AnimatorControllerParameter",{"System.Int32"},false,M(Parameter)},
        {"CrossFade","System.Void",{"System.Int32","System.Single","System.Int32","System.Single","System.Single"},false,M(Fade)},
        {"Play","System.Void",{"System.Int32","System.Int32","System.Single"},false,M(Play)},
        {"Play","System.Void",{"System.String","System.Int32","System.Single"},false,{}},
        {"Play","System.Void",{"System.Int32"},false,{}}}};
    DeclaredType free_style{"RPG.GameCore","FreeStyleComponent",{
        {"PlayInitFreeStyle","System.Void",{"System.UInt32"},false,M(Npc)},
        {"PlayAnimState","System.Void",{"System.Int32","System.Single","System.Single"},false,M(State)}}};
    DeclaredType model_type{"RPG.GameCore","CharacterModelComponent",{
        {"SetReplaceAnimatorController","System.Void",{"System.String","System.String"},false,M(Path)}}};
    DeclaredType manager_type{"RPG.Client","AnimatorOverrideManager",{
        {"SetReplaceAnimatorController","System.Void",{"UnityEngine.RuntimeAnimatorController","System.String"},false,M(Object)}}};
    DeclaredType loader_type{"","NLMPFKPGCNA",{
        {"KEMOCLLBELE","RPG.Client.AnimatorOverrideManager",{},false,M(Manager)}}};
    DeclaredType builtins{"","NNMPPHHHEIK",{{"PCJGGHBFMPI","System.Int32",{"RPG.GameCore.BuildInAnimParam"},true,M(ReadyHash)}}};
    DeclaredType parameter_decl{"UnityEngine","AnimatorControllerParameter",{
        {"get_nameHash","System.Int32",{},false,M(ParameterHash)},
        {"get_type","UnityEngine.AnimatorControllerParameterType",{},false,M(ParameterType)}}};
    MetadataResolver declarations({anim,free_style,model_type,manager_type,loader_type,builtins,parameter_decl});
    const auto methods=ResolveAnimationControlMethods(declarations);
    Check(methods.binding_errors.empty()); AnimationControl calls(methods);
    Check(calls.LayerIndex(&animator,&name).value==3 && calls.LayerWeight(&animator,3).value==0.75f);
    Check(calls.SetLayerWeight(&animator,3,0).returned && calls.Evaluate(&animator,0).returned);
    Check(!calls.LayerIndex(&animator,nullptr).read.returned && !calls.SetLayerWeight(&animator,-1,0).returned);
    Check(calls.StateHash(&name).read.returned && calls.StateHash(&name).value==-731);
    Check(calls.HasState(&animator,2,-731).value && !calls.HasState(&animator,2,1).value);
    Check(calls.Controller(&animator).object==&controller);
    Check(calls.ResolveStateName(&animator,-731).object==&name && !calls.ResolveStateName(nullptr,-731).call.returned);
    Check(calls.LayerName(&animator,0).object==&name && !calls.LayerName(&animator,-1).call.returned);
    MetadataResolver motion_declarations({{"","BGOBCKLPEMK",{
        {"HELJCOPGIMD","System.Single",{"System.Single"},false,M(FlightHeight)},
        {"HKGEJFOLDAN","System.Single",{},false,M(FallHeight)}}}});
    std::vector<DeclaredField> motion_fields{{"","BGOBCKLPEMK","MFIFHCICOGN","System.Single",offsetof(Motion,clock),0,true}};
    auto mm=methods;ResolveHitMotionReads(mm,motion_declarations,motion_fields);AnimationControl movement(mm);
    Check(mm.binding_errors.empty() && movement.HitMotionHeight(&motion).value==.6f && flight_reads==1 && fall_reads==1);
    flight_height=.2f;Check(movement.HitMotionHeight(&motion).value==.2f && fall_reads==1);
    Check(!movement.HitMotionHeight(nullptr).read.returned);
    motion_fields.front().flags=0x10;ResolveHitMotionReads(mm,motion_declarations,motion_fields);
    Check(!AnimationControl(mm).HitMotionHeight(&motion).read.returned);
    Check(calls.ReadyTransitParameter().value==712 && calls.HasFloatParameter(&animator,712).value);
    Check(!calls.HasFloatParameter(&animator,713).value && calls.HasFloatParameter(&animator,713).read.returned);
    parameter_kind=3;Check(!calls.HasFloatParameter(&animator,712).value);parameter_kind=1;
    Check(calls.FloatParameter(&animator,712).value==0.7f && calls.SetFloatParameter(&animator,712,-1).returned);
    Check(calls.FloatParameter(&animator,712).value==-1 && !calls.SetFloatParameter(nullptr,712,1).returned);
    Check(calls.LayerCount(&animator).value==5 && calls.CrossFade(&animator,-731,0.15f,2).returned);
    const auto sample=calls.CurrentState(&animator);
    Check(sample.read.returned && sample.short_name_hash==-731 && sample.normalized_time==0.375f);
    Check(calls.NextState(&animator).read.returned && calls.NextState(&animator).short_name_hash==-731);
    Check(calls.PlayAnimator(&animator,-731,2,0.25f).returned && played==1);
    Check(calls.PlayNpcMotion(&style,0xF0000011u).returned && npc_played==1);
    Check(calls.PlayFreeStyleState(&style,-731,0.125f,0.75f).returned && state_played==1);
    Check(calls.ReplacePath(&model,&path).returned && calls.ReplacePath(&model,nullptr).returned && path_calls==2);
    Check(calls.OverrideManager(&loader).object==&manager);
    Check(calls.ReplaceObject(&manager,&controller,&tag).returned && calls.ReplaceObject(&manager,nullptr,&tag).returned && object_calls==2);
    Check(!calls.ReplaceObject(&manager,nullptr,nullptr).returned && object_calls==2);
    Check(!calls.PlayAnimator(nullptr,-731,2,0.25f).returned && !calls.StateHash(nullptr).read.returned);
    fault=true; Check(calls.PlayAnimator(&animator,-731,2,0.25f).exception==0xE0670001); fault=false;
    // Unavailable loader does not disable any unrelated motion entry.
    MetadataResolver partial({anim,free_style,model_type,manager_type,builtins,parameter_decl});
    const auto incomplete=ResolveAnimationControlMethods(partial);
    Check(incomplete.binding_errors.size()==1 && !incomplete.override_manager.code && incomplete.play.code);
    Check(AnimationControl(incomplete).PlayNpcMotion(&style,0xF0000011u).returned && npc_played==2);
    MetadataResolver blink_declarations({
        {"","PEPNFLNCDHG",{{"PIDDPGGHPKH","System.Void",{"PEPNFLNCDHG.BlinkEnableSource","System.Boolean"},false,M(BlinkSet)}}},
        {"","EONKJJEBGLN",{
            {"ACMKLFOMPFC","System.Boolean",{"System.Int32"},false,M(BlinkValue)},
            {"OLHPIIMLHFM","System.Boolean",{"System.Int32"},false,M(BlinkActive)}}}});
    std::vector<DeclaredField> blink_fields{{"","PEPNFLNCDHG","NAIALJJNIGG","EONKJJEBGLN",offsetof(Emotion,mask),0,true}};
    auto bm=methods;ResolveBlinkControl(bm,blink_declarations,blink_fields);AnimationControl blink(bm);
    Check(bm.binding_errors.empty() && !blink.BlinkSource(&emotion,3).active.value);
    blink_mask.active[1]=true;blink_mask.value[1]=false;
    Check(blink.SetBlinkSource(&emotion,3,false).returned && blink.BlinkSource(&emotion,3).active.value);
    Check(!blink.BlinkSource(&emotion,3).enabled.value && blink.SetBlinkSource(&emotion,3,true).returned);
    Check(blink.BlinkSource(&emotion,3).enabled.value && blink_mask.active[1] && !blink_mask.value[1]);
    blink_fields.front().flags=0x10;ResolveBlinkControl(bm,blink_declarations,blink_fields);
    Check(!bm.blink.mask_available && !AnimationControl(bm).BlinkSource(&emotion,3).active.read.returned);
    std::cout<<"Animation control API: declared overloads, static hash, integer/float ABI, independent bindings, scoped override removal; no game acceptance.\n";
}
