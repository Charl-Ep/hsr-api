#include "CatalogReader.hpp"
#include <type_traits>

namespace hksr::sdk_v1 {
ConfigValue<HitEffectRules> OfficialCatalog::ReadHitEffectRules(const std::string& path) {
    ConfigValue<HitEffectRules> out;auto& s=*state_;State::Scope scope(s);
    try {
        auto* force=scope.Keep(s.StaticValue(scope,"RPG.Client","MonoEffectManager","ForceEntityMutex"));
        out.value.force_entity=Read<std::uint8_t>(force,Field(s.metadata.ReadObjectFields(force),"m_value","System.Boolean"))!=0;
        if(!out.value.force_entity) {
            auto* config=scope.Keep(s.StaticValue(scope,"RPG.Client","MonoEffectManager","EffectJsonConfig"));
            auto* map=Read<void*>(config,Field(s.metadata.ReadObjectFields(config),"EffectMutexMap",
                "System.Collections.Generic.Dictionary<System.String,RPG.GameCore.EffectMutexConfigData>"));
            Require(map,"effect mutex map absent");scope.Keep({{true,0},map});
            auto* key=scope.Keep(s.objects.NewStringUtf8(path.c_str()));
            const auto type=s.metadata.ReadObjectType(map);bool present=false;
            Require(native::Call(Instance(type,"ContainsKey","System.Boolean",{"System.String"}),&present,map,key).returned,"effect mutex key query failed");
            if(present) {
                auto* row=scope.Keep(s.Call(Instance(type,"get_Item","RPG.GameCore.EffectMutexConfigData",{"System.String"}),map,key));
                const auto fields=s.metadata.ReadObjectFields(row);
                EffectMutexRule rule;
                rule.interval=Read<float>(row,Field(fields,"MinMutexTime","System.Single"));
                rule.scope=static_cast<EffectMutexScope>(Read<std::int32_t>(row,Field(fields,"MinMutexType","RPG.GameCore.EffectMutexType")));
                // Native CheckEffectMutex consumes the UInt32 field as Int32.
                rule.maximum=Read<std::int32_t>(row,Field(fields,"MaxMutexCount","System.UInt32"));
                out.value.configured=rule;
            }
        }
        out.available=true;
    }catch(const std::exception& e){out.error=e.what();}
    return out;
}
VisualReactionInput OfficialCatalog::ReadDefaultHitNames() {
    VisualReactionInput out;out.present=true;auto& s=*state_;
    auto read=[&](TextInput& target,std::uint8_t category) {
        try {
            State::Scope scope(s);
            const auto lookup=s.methods.Resolve("",detail::catalog_bindings::animation_names,detail::catalog_bindings::default_reaction_name,
                "System.String",{"RPG.GameCore.AnimCategory"},MethodKind::Static);
            const auto result=s.Call(lookup,category);
            Require(result.call.returned,"default hit name query failed");
            target.value=result.object?s.Text(scope.Keep(result)):std::string();target.available=true;
        }catch(const std::exception& error){target.error=error.what();}
    };
    // Categories consumed by the qualified OnHit entry and name resolver.
    // Names are returned by the existing official pure lookup, not copied here.
    read(out.hit,1);read(out.hit_h,2);read(out.hit_fly,7);read(out.fly_hit,9);
    return out;
}
CharacterHitInput OfficialCatalog::ReadCharacterHitInput(const std::string& path) {
    CharacterHitInput out;out.source=path;auto& s=*state_;State::Scope scope(s);
    void* config=nullptr;std::vector<DeclaredField> fields;
    try {config=s.Config("CharacterConfig",path,scope);fields=s.metadata.ReadObjectFields(config);out.loaded=true;}
    catch(const std::exception& error){out.issues.push_back({-1,path,error.what()});return out;}
    auto capture=[&](auto& target,const char* field,auto read) {
        try {target.value=read();target.available=true;}
        catch(const std::exception& error){target.error=error.what();out.issues.push_back({-1,field,error.what()});}
    };
    capture(out.resilience,"Resilience",[&]{return Read<std::int32_t>(config,Field(fields,"Resilience","RPG.GameCore.ResilienceEnum"));});
    capture(out.repeat_occur_at,"RepeatOccurAnimWhenBeHitNormalizedTime",[&]{
        return Read<float>(config,Field(fields,"RepeatOccurAnimWhenBeHitNormalizedTime","System.Single"));
    });
    capture(out.shape,"HitBox",[&] {
        HitShapeParameters shape;
        shape.kind=static_cast<HitShapeKind>(Read<std::int32_t>(config,Field(fields,"HitBoxType","RPG.GameCore.CharacterCollisionType")));
        shape.width=Read<float>(config,Field(fields,"HitBoxWidth","System.Single"));
        shape.height=Read<float>(config,Field(fields,"HitBoxHeight","System.Single"));
        shape.offset=Read<Position3>(config,Field(fields,"HitBoxOffset","RPG.MVector3"));
        return shape;
    });
    capture(out.hit_box_attach_point,"HitBoxAttachPoint",[&]{return s.String(config,fields,"HitBoxAttachPoint");});
    void* phase=nullptr;
    capture(out.phase_present,"PhaseAnimConfig",[&] {
        phase=Read<void*>(config,Field(fields,"PhaseAnimConfig","RPG.GameCore.CharacterPhaseAnimConfig"));
        if(phase)scope.Keep({{true,0},phase});return phase!=nullptr;
    });
    if(!out.phase_present.available || !phase)return out;
    out.phase.present=true;
    try {fields=s.metadata.ReadObjectFields(phase);}
    catch(const std::exception& error){out.issues.push_back({-1,"phase fields",error.what()});return out;}
    auto text=[&](TextInput& target,const char* name){capture(target,name,[&]{return s.String(phase,fields,name);});};
    text(out.phase.hit,"Hit");text(out.phase.hit_h,"HitH");text(out.phase.hit_fly,"HitFly");text(out.phase.fly_hit,"FlyHit");
    capture(out.phase.force_hit_h,"ForceHitH",[&]{return Read<std::uint8_t>(phase,Field(fields,"ForceHitH","System.Boolean"))!=0;});
    capture(out.phase.hit_weight,"OverrideHitAnimWeight",[&]{return Read<float>(phase,Field(fields,"OverrideHitAnimWeight","System.Single"));});
    return out;
}
HitAnimationDefaults OfficialCatalog::ReadHitAnimationDefaults() {
    HitAnimationDefaults out;auto& s=*state_;State::Scope scope(s);
    void* config=nullptr;std::vector<DeclaredField> fields;
    try {config=s.GlobalVisualConfig(scope);fields=s.metadata.ReadObjectFields(config);}
    catch(const std::exception& error) {
        out.resilience_weights.error=out.avatar_weight.error=out.monster_weight.error=out.minimum_frame_interval.error=error.what();
        return out;
    }
    auto scalar=[&](auto& target,const char* name,const char* type) {
        try {target.value=Read<std::decay_t<decltype(target.value)>>(config,Field(fields,name,type));target.available=true;}
        catch(const std::exception& error){target.error=error.what();}
    };
    scalar(out.avatar_weight,"AvatarHitAdditiveWeightBase","System.Single");
    scalar(out.monster_weight,"MonsterHitAdditiveWeightBase","System.Single");
    scalar(out.minimum_frame_interval,"HitFrameMiniInterval","System.Int32");
    try {
        auto* array=Read<void*>(config,Field(fields,"ResilienceHitAdditiveWeights","System.Single[]"));
        Require(array,"hit resilience weight array is null");scope.Keep({{true,0},array});
        const auto count=s.arrays.Length(array);Require(count.read.returned && count.value>=0,"hit weight count unread");
        std::vector<float> values;
        for(int i=0;i<count.value;++i) {
            State::Scope item_scope(s);auto* item=item_scope.Keep(s.arrays.Get(array,i));
            values.push_back(Read<float>(item,Field(s.metadata.ReadObjectFields(item),"m_value","System.Single")));
        }
        out.resilience_weights.value=std::move(values);out.resilience_weights.available=true;
    }catch(const std::exception& error){out.resilience_weights.error=error.what();}
    return out;
}
}
