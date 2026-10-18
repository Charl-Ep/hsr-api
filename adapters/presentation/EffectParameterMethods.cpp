#include <hksr/presentation/effects/EffectAnimatorParameters.hpp>
#include <exception>

namespace hksr::sdk_v1 {
EffectParameterMethods ResolveEffectAnimatorParameters460(MethodResolver& metadata) {
    EffectParameterMethods out;
    auto get=[&](const char* ns,const char* type,const char* name,const char* result,
        std::initializer_list<const char*> args,MethodKind kind=MethodKind::Instance) {
        try{return metadata.Resolve(ns,type,name,result,args,kind);}
        catch(const std::exception& e){out.binding_errors.push_back(std::string(type)+"."+name+": "+e.what());return Method{};}
    };
    out.game_object=get("UnityEngine","Component","get_gameObject","UnityEngine.GameObject",{});
    out.first_child=get("UnityEngine","GameObject","GetComponentInChildren","UnityEngine.Component",{"System.Type","System.Boolean"});
    out.children=get("UnityEngine","GameObject","GetComponentsInChildren","UnityEngine.Component[]",{"System.Type","System.Boolean"});
    out.alive=get("UnityEngine","Object","op_Implicit","System.Boolean",{"UnityEngine.Object"},MethodKind::Static);
    out.parameters=get("UnityEngine","Animator","get_parameters","UnityEngine.AnimatorControllerParameter[]",{});
    out.parameter_name=get("UnityEngine","AnimatorControllerParameter","get_name","System.String",{});
    out.parameter_type=get("UnityEngine","AnimatorControllerParameter","get_type","UnityEngine.AnimatorControllerParameterType",{});
    out.string_equal=get("System","String","op_Equality","System.Boolean",{"System.String","System.String"},MethodKind::Static);
    out.set_float=get("UnityEngine","Animator","SetFloat","System.Void",{"System.String","System.Single"});
    out.set_integer=get("UnityEngine","Animator","SetInteger","System.Void",{"System.String","System.Int32"});
    out.set_bool=get("UnityEngine","Animator","SetBool","System.Void",{"System.String","System.Boolean"});
    out.set_trigger=get("UnityEngine","Animator","SetTrigger","System.Void",{"System.String"});
    return out;
}
}
