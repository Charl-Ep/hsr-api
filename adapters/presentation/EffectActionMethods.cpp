#include <hksr/presentation/effects/EffectActions.hpp>
#include <exception>

namespace hksr::sdk_v1 {
EffectActionMethods ResolveEffectActions460(MethodResolver& metadata) {
    EffectActionMethods out;
    auto get=[&](const char* ns,const char* type,const char* name,const char* result,
        std::initializer_list<const char*> args,MethodKind kind=MethodKind::Instance) {
        try{return metadata.Resolve(ns,type,name,result,args,kind);}
        catch(const std::exception& e){out.binding_errors.push_back(std::string(type)+"."+name+": "+e.what());return Method{};}
    };
    out.children=get("UnityEngine","Component","GetComponentsInChildren","UnityEngine.Component[]",{"System.Type","System.Boolean"});
    out.alive=get("UnityEngine","Object","op_Implicit","System.Boolean",{"UnityEngine.Object"},MethodKind::Static);
    out.set_trigger=get("UnityEngine","Animator","SetTrigger","System.Void",{"System.String"});
    out.reset_trigger=get("UnityEngine","Animator","ResetTrigger","System.Void",{"System.String"});
    out.remove_input=get("RPG.Client","MonoEffectManager","GetRemoveEffectParam","PCHFCKDEAPK",
        {"RPG.GameCore.GameEntity","System.String","System.String","System.String","System.Boolean","System.Boolean",
         "System.UInt32","RPG.GameCore.MonoEffectFlag[]","System.Boolean"},MethodKind::Static);
    out.remove=get("RPG.Client","MonoEffectManager","RemoveEffect","System.Boolean",{"PCHFCKDEAPK","System.Boolean"});
    out.release_input=get("RPG.Client","MonoEffectManager","ReleaseRemoveEffectParam","System.Void",{"PCHFCKDEAPK"},MethodKind::Static);
    return out;
}
}
