#include <hksr/presentation/effects/EffectControl.hpp>
#include <exception>

namespace hksr::sdk_v1 {
EffectControlMethods ResolveEffectControl460(MethodResolver& metadata) {
    EffectControlMethods out;
    auto get=[&](const char* ns,const char* type,const char* name,const char* result,
        std::initializer_list<const char*> args,MethodKind kind=MethodKind::Instance) {
        try{return metadata.Resolve(ns,type,name,result,args,kind);}
        catch(const std::exception& e){out.binding_errors.push_back(std::string(type)+"."+name+": "+e.what());return Method{};}
    };
    out.component=get("UnityEngine","Component","GetComponent","UnityEngine.Component",{"System.Type"});
    out.alive=get("UnityEngine","Object","op_Implicit","System.Boolean",{"UnityEngine.Object"},MethodKind::Static);
    out.progress_behavior=get("RPG.Client","MonoEffectPluginProgress","get_Behavior","LMGBJIFBAAN",{});
    out.progress_owner=get("","LMGBJIFBAAN","HPNDIIPEFLJ","RPG.Client.MonoEffectPluginProgress",{});
    out.progress_set=get("","LMGBJIFBAAN","EJDAKAECODE","System.Void",{"System.Single","System.Single"});
    out.animator_play=get("UnityEngine","Animator","Play","System.Void",{"System.Int32","System.Int32","System.Single"});
    out.animator_cross_fade=get("UnityEngine","Animator","CrossFade","System.Void",
        {"System.Int32","System.Single","System.Int32","System.Single","System.Single"});
    out.animator_state_hash=get("UnityEngine","Animator","StringToHash","System.Int32",{"System.String"},MethodKind::Static);
    return out;
}
}
