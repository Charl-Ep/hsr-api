#include <hksr/presentation/effects/EffectEntityFollow.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
EffectMotionTargetBinding ResolveEffectMotionTarget460(MethodResolver& metadata,const std::vector<DeclaredField>& fields) {
    auto field=[&](const char* name,const char* type) {
        const DeclaredField* found=nullptr;
        for(const auto& row:fields)if(row.name==name) {
            if(found || row.type!=type || !row.flags_observed || (row.flags&0x10))
                throw std::invalid_argument(std::string("effect motion target field differs: ")+name);
            found=&row;
        }
        if(!found)throw std::invalid_argument(std::string("effect motion target field absent: ")+name);
        return found->offset;
    };
    return {metadata.Resolve("RPG.GameCore","GameEntity","GetComponent","RPG.GameCore.GameComponentBase",{"System.Type"}),
        field("_OwnerRef","RPG.GameCore.GameEntity"),field("<IsAttachMotionWithEffect>k__BackingField","System.Boolean"),
        field("PGALAIJOICA","FGNNHFLGJBK"),field("MIHFBGNKBJA","AKDDNBHNGON")};
}
EffectEntityFollowMethods ResolveEffectEntityFollow460(MethodResolver& metadata) {
    EffectEntityFollowMethods out;
    auto get=[&](const char* ns,const char* type,const char* name,const char* result,
        std::initializer_list<const char*> args,MethodKind kind=MethodKind::Instance) {
        try{return metadata.Resolve(ns,type,name,result,args,kind);}
        catch(const std::exception& e){out.binding_errors.push_back(std::string(type)+"."+name+": "+e.what());return Method{};}
    };
    out.component=get("UnityEngine","Component","GetComponent","UnityEngine.Component",{"System.Type"});
    out.alive=get("UnityEngine","Object","op_Implicit","System.Boolean",{"UnityEngine.Object"},MethodKind::Static);
    out.behavior=get("RPG.Client","MonoEffectPluginEntityFollowEffect","get_Behavior","GIMGIMFGBFF",{});
    out.behavior_owner=get("","GIMGIMFGBFF","HPNDIIPEFLJ","RPG.Client.MonoEffectPluginEntityFollowEffect",{});
    out.bind=get("","GIMGIMFGBFF","CDEGKEKEHID","System.Void",{"System.Collections.Generic.IList<RPG.GameCore.GameEntity>"});
    out.clear=get("","GIMGIMFGBFF","CHBEGMHKJKP","System.Void",{});
    return out;
}
EffectEntityFollowFields ResolveEffectEntityFollowFields460(const std::vector<DeclaredField>& effect,
    const std::vector<DeclaredField>& plugin,const std::vector<DeclaredField>& behavior) {
    auto field=[](const auto& rows,const char* owner,const char* name,const char* type) {
        const DeclaredField* found=nullptr;
        for(const auto& row:rows)if(row.owner_name==owner && row.name==name) {
            if(found || row.type!=type || !row.flags_observed || (row.flags&0x10))
                throw std::invalid_argument(std::string(owner)+"."+name+" declaration differs");
            found=&row;
        }
        if(!found)throw std::invalid_argument(std::string(owner)+"."+name+" declaration absent");
        return found->offset;
    };
    return {field(effect,"MonoEffect","TriggerParams","RPG.Client.TriggerEffectParams"),
        field(plugin,"MonoEffectPluginBase","_OwnerEffect","RPG.Client.MonoEffect"),
        field(behavior,"GIMGIMFGBFF","PJFPEKPDKIH","System.Collections.Generic.List<RPG.GameCore.GameEntity>")};
}
}
