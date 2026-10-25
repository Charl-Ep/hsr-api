#include <hksr/animation/AnimationControl.hpp>
#include <exception>
#include <hksr/native/Invocation.hpp>
#include <array>
#include <cstring>

namespace hksr::sdk_v1 {
namespace {
AnimatorStateObservation StateInfo(Method method,void* animator,int layer,int index) {
    AnimatorStateObservation out;
    std::array<unsigned char,36> bytes{};
    if(animator) out.read=native::CallVoid(method,animator,layer,index,bytes.data());
    if(out.read.returned) {
        std::memcpy(&out.short_name_hash,bytes.data(),sizeof(out.short_name_hash));
        std::memcpy(&out.normalized_time,bytes.data()+12,sizeof(out.normalized_time));
        std::memcpy(&out.full_path_hash,bytes.data()+8,sizeof(out.full_path_hash));
        int loop=0;std::memcpy(&loop,bytes.data()+32,sizeof(loop));out.loop=loop!=0;
    }
    return out;
}
}
AnimatorStateObservation AnimationControl::CurrentState(void* animator,int layer) const {
    return StateInfo(methods_->state_info,animator,layer,0);
}
AnimatorStateObservation AnimationControl::NextState(void* animator,int layer) const {
    return StateInfo(methods_->state_info,animator,layer,1);
}
namespace {
ControllerLinkField Link(const std::vector<DeclaredField>& fields,const char* type) {
    ControllerLinkField out; unsigned matches=0;
    for(const auto& f:fields) if((f.type==type || f.type==std::string(".")+type) &&
        f.flags_observed && !(f.flags&0x10)) { ++matches; out.offset=f.offset; }
    out.available=matches==1;
    if(!out.available) out.error=std::string(type)+": expected one current instance reference";
    return out;
}
}
ControllerLinkField ResolveModelControllerLoader(const std::vector<DeclaredField>& fields) {
    return Link(fields,"NLMPFKPGCNA");
}
ControllerLinkField ResolveControllerLoaderModel(const std::vector<DeclaredField>& fields) {
    return Link(fields,"RPG.GameCore.CharacterModelComponent");
}
ControllerLinkField ResolveControllerLoaderManager(const std::vector<DeclaredField>& fields) {
    return Link(fields,"RPG.Client.AnimatorOverrideManager");
}
AnimationControlMethods ResolveAnimationControlMethods(MethodResolver& resolver) {
    AnimationControlMethods out;
    auto get=[&](const char* ns,const char* type,const char* name,const char* result,
        std::initializer_list<const char*> params,MethodKind kind=MethodKind::Instance) {
        try { return resolver.Resolve(ns,type,name,result,params,kind); }
        catch(const std::exception& e) { out.binding_errors.push_back(std::string(type)+"."+name+": "+e.what()); return Method{}; }
    };
    out.hash=get("UnityEngine","Animator","StringToHash","System.Int32",{"System.String"},MethodKind::Static);
    out.resolve_hash=get("UnityEngine","Animator","ResolveHash","System.String",{"System.Int32"});
    out.has_state=get("UnityEngine","Animator","HasState","System.Boolean",{"System.Int32","System.Int32"});
    out.play=get("UnityEngine","Animator","Play","System.Void",{"System.Int32","System.Int32","System.Single"});
    out.controller=get("UnityEngine","Animator","get_runtimeAnimatorController","UnityEngine.RuntimeAnimatorController",{});
    out.npc_motion=get("RPG.GameCore","FreeStyleComponent","PlayInitFreeStyle","System.Void",{"System.UInt32"});
    out.free_style_state=get("RPG.GameCore","FreeStyleComponent","PlayAnimState","System.Void",{"System.Int32","System.Single","System.Single"});
    out.replace_path=get("RPG.GameCore","CharacterModelComponent","SetReplaceAnimatorController","System.Void",{"System.String","System.String"});
    out.replace_object=get("RPG.Client","AnimatorOverrideManager","SetReplaceAnimatorController","System.Void",{"UnityEngine.RuntimeAnimatorController","System.String"});
    out.override_manager=get("","NLMPFKPGCNA","KEMOCLLBELE","RPG.Client.AnimatorOverrideManager",{});
    out.state_info=get("UnityEngine","Animator","GetAnimatorStateInfo","System.Void",{"System.Int32","UnityEngine.StateInfoIndex","UnityEngine.AnimatorStateInfo&"});
    out.in_transition=get("UnityEngine","Animator","IsInTransition","System.Boolean",{"System.Int32"});
    out.layer_count=get("UnityEngine","Animator","get_layerCount","System.Int32",{});
    out.layer_index=get("UnityEngine","Animator","GetLayerIndex","System.Int32",{"System.String"});
    out.layer_name=get("UnityEngine","Animator","GetLayerName","System.String",{"System.Int32"});
    out.layer_weight=get("UnityEngine","Animator","GetLayerWeight","System.Single",{"System.Int32"});
    out.set_layer_weight=get("UnityEngine","Animator","SetLayerWeight","System.Void",{"System.Int32","System.Single"});
    out.update=get("UnityEngine","Animator","Update","System.Void",{"System.Single"});
    out.cross_fade=get("UnityEngine","Animator","CrossFade","System.Void",{"System.Int32","System.Single","System.Int32","System.Single","System.Single"});
    out.ready_parameter_hash=get("","NNMPPHHHEIK","PCJGGHBFMPI","System.Int32",{"RPG.GameCore.BuildInAnimParam"},MethodKind::Static);
    out.float_get=get("UnityEngine","Animator","GetFloat","System.Single",{"System.Int32"});
    out.float_set=get("UnityEngine","Animator","SetFloat","System.Void",{"System.Int32","System.Single"});
    out.parameter_count=get("UnityEngine","Animator","get_parameterCount","System.Int32",{});
    out.parameter_at=get("UnityEngine","Animator","GetParameter","UnityEngine.AnimatorControllerParameter",{"System.Int32"});
    out.parameter_hash=get("UnityEngine","AnimatorControllerParameter","get_nameHash","System.Int32",{});
    out.parameter_type=get("UnityEngine","AnimatorControllerParameter","get_type","UnityEngine.AnimatorControllerParameterType",{});
    return out;
}
void ResolveHitMotionReads(AnimationControlMethods& out,MethodResolver& resolver,const std::vector<DeclaredField>& fields) {
    try {out.hit_motion.flight_height=resolver.Resolve("","BGOBCKLPEMK","HELJCOPGIMD","System.Single",{"System.Single"});}
    catch(const std::exception& e){out.binding_errors.push_back(std::string("hit flight height: ")+e.what());}
    try {out.hit_motion.fall_height=resolver.Resolve("","BGOBCKLPEMK","HKGEJFOLDAN","System.Single",{});}
    catch(const std::exception& e){out.binding_errors.push_back(std::string("hit fall height: ")+e.what());}
    unsigned count=0;
    for(const auto& field:fields)if(field.owner_name=="BGOBCKLPEMK" && field.name=="MFIFHCICOGN" &&
        field.type=="System.Single" && field.flags_observed && !(field.flags&0x10)) {
        ++count;out.hit_motion.clock_offset=field.offset;
    }
    out.hit_motion.clock_available=count==1;
    if(!out.hit_motion.clock_available)out.binding_errors.push_back("hit motion clock field unavailable/ambiguous");
}
void ResolveBlinkControl(AnimationControlMethods& out,MethodResolver& resolver,const std::vector<DeclaredField>& fields) {
    auto get=[&](const char* type,const char* name,const char* result,std::initializer_list<const char*> args) {
        try{return resolver.Resolve("",type,name,result,args);}
        catch(const std::exception& e){out.binding_errors.push_back(std::string("blink ")+type+"."+name+": "+e.what());return Method{};}
    };
    out.blink.set_source=get("PEPNFLNCDHG","PIDDPGGHPKH","System.Void",{"PEPNFLNCDHG.BlinkEnableSource","System.Boolean"});
    out.blink.source_value=get("EONKJJEBGLN","ACMKLFOMPFC","System.Boolean",{"System.Int32"});
    out.blink.source_active=get("EONKJJEBGLN","OLHPIIMLHFM","System.Boolean",{"System.Int32"});
    unsigned matches=0;
    for(const auto& f:fields)if(f.owner_name=="PEPNFLNCDHG" && f.name=="NAIALJJNIGG" &&
        f.type=="EONKJJEBGLN" && f.flags_observed && !(f.flags&0x10)){++matches;out.blink.mask_offset=f.offset;}
    out.blink.mask_available=matches==1;
    if(!out.blink.mask_available)out.binding_errors.push_back("blink source mask field unavailable/ambiguous");
}
}
