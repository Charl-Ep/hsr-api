#include <hksr/presentation/effects/EffectAccess.hpp>
#include <exception>

namespace hksr::sdk_v1 {
EffectAccessMethods ResolveEffectAccess460(MethodResolver& metadata) {
    EffectAccessMethods out;
    auto get=[&](const char* space,const char* type,const char* name,const char* result,
        std::initializer_list<const char*> args,MethodKind kind=MethodKind::Instance) {
        try{return metadata.Resolve(space,type,name,result,args,kind);}
        catch(const std::exception& e){out.binding_errors.push_back(std::string(type)+"."+name+": "+e.what());return Method{};}
    };
    out.map_path=get("RPG.GameCore","GameEntityExtensions","MapResPath","System.String",
        {"RPG.GameCore.GameEntity","System.String"},MethodKind::Static);
    out.acquire=get("RPG.Client","MonoEffectManager","GetTriggerEffectParamsRef","RPG.Client.TriggerEffectParamsRef",{"System.String"},MethodKind::Static);
    out.submit=get("RPG.Client","MonoEffectManager","FireEffectAsync","System.Void",{"RPG.Client.TriggerEffectParams"});
    out.remove=get("RPG.Client","MonoEffectManager","RemoveEffectByTriggerParamsRef","System.Boolean",{"RPG.Client.TriggerEffectParamsRef","System.Boolean"});
    out.release=get("RPG.Client","MonoEffectManager","ReleaseTriggerEffectParam","System.Void",{"RPG.Client.TriggerEffectParams"},MethodKind::Static);
    out.id=get("RPG.Client","TriggerEffectParams","get_UniqueEffectID","System.UInt32",{});
    out.released=get("RPG.Client","TriggerEffectParams","get_IsReleased","System.Boolean",{});
    out.state=get("RPG.Client","TriggerEffectParams","get_AliveState","RPG.Client.MonoEffectAliveState",{});
    out.instance=get("RPG.Client","TriggerEffectParams","get_EffectInstance","RPG.Client.MonoEffect",{});
    out.path_empty=get("System","String","IsNullOrEmpty","System.Boolean",{"System.String"},MethodKind::Static);
    return out;
}
}
