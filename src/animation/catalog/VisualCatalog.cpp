#include "CatalogReader.hpp"

namespace hksr::sdk_v1 {
CatalogBatch<VisualBehaviorInput> OfficialCatalog::State::VisualRows(void* config,const char* array_name,void* phase_config) {
        Scope preparation_scope(*this);
        void* private_table=nullptr;bool preparation_attempted=false;
        Method clone_method{},prepare_method{};std::string preparation_error;
        auto table=[&]() {
            if(!preparation_attempted) {
                preparation_attempted=true;
                try {
                    clone_method=methods.Resolve("System","Object","MemberwiseClone","System.Object",{});
                    const auto ctor=methods.Resolve("",detail::catalog_bindings::visual_table,".ctor","System.Void",{});
                    prepare_method=methods.Resolve("",detail::catalog_bindings::visual_table,detail::catalog_bindings::prepare_visual_row,"System.Void",
                        {"RPG.GameCore.ModifierBehaviorVisual","System.Int32","System.Int32"});
                    auto* candidate=preparation_scope.Keep(objects.AllocateObject("",detail::catalog_bindings::visual_table));
                    Require(native::CallVoid(ctor,candidate).returned,"private visual table constructor failed");
                    private_table=candidate;
                }catch(const std::exception& e){preparation_error=e.what();}
            }
            if(!private_table)throw std::runtime_error(preparation_error);
            return private_table;
        };
        return Rows<VisualBehaviorInput>(config,metadata.ReadObjectFields(config),array_name,
            [&](void* row,const auto& fields,int index) {
                VisualBehaviorInput value; value.source_index=index;
                Scope prepared_scope(*this);void* prepared_row=nullptr;
                auto capture=[&](auto& out,const char* name,auto read) {
                    try {out.value=read();out.available=true;}
                    catch(const std::exception& e){out.error=e.what();value.issues.push_back({index,name,e.what()});}
                };
                auto string=[&](TextInput& out,const char* name){capture(out,name,[&]{return String(row,fields,name);});};
                auto number=[&](ConfigValue<float>& out,const char* name){capture(out,name,[&]{return Read<float>(row,Field(fields,name,"System.Single"));});};
                auto boolean=[&](ConfigValue<bool>& out,const char* name){capture(out,name,[&]{return Read<std::uint8_t>(row,Field(fields,name,"System.Boolean"))!=0;});};
                capture(value.flag,"Flag",[&]{return Read<std::int32_t>(row,Field(fields,"Flag","RPG.GameCore.ModifierBehaviorFlag"));});
                try {
                    Require(value.flag.available,"visual flag unread; original fields retained");
                    auto* owner=table();
                    prepared_row=prepared_scope.Keep(Call(clone_method,row));
                    Require(prepared_row!=row,"visual clone aliases original config");
                    // The official function writes only the copied row's derived
                    // fields and this private table's index. ReactionAnims is read
                    // later, never written; an absent floating object is allocated
                    // into the copied row only. No shared singleton is registered.
                    Require(native::CallVoid(prepare_method,owner,prepared_row,value.flag.value,index).returned,
                        "official visual row preparation failed");
                    value.prepared=true;
                }catch(const std::exception& e){value.issues.push_back({index,"prepare private visual row",e.what()});}
                capture(value.material_effect,"MaterialEffect",[&]{return Read<std::int32_t>(row,Field(fields,"MaterialEffect","RPG.GameCore.CharacterMaterialEffectType"));});
                string(value.priority,"ModifierBehaviorVisualPriority");string(value.emotion,"EmoAnimName");
                string(value.floating_fail_recover,"FloatingFailAnimRecover");
                auto phase=[&](VisualPhaseInput& out,const char* name,const char* transition,const char* category) {
                    string(out.state,name);number(out.transition,transition);
                    capture(out.source_category,category,[&]{return Read<std::uint8_t>(row,Field(fields,category,"RPG.GameCore.AnimCategory"));});
                    capture(out.category,category,[&]{
                        Require(value.prepared,"official visual row not prepared");
                        return Read<std::uint8_t>(prepared_row,Field(fields,category,"RPG.GameCore.AnimCategory"));
                    });
                };
                phase(value.occur,"AnimOccur","AnimOccurTransition","AnimOccurCat");
                phase(value.loop,"AnimLoop","AnimLoopTransition","AnimLoopCat");
                phase(value.recover,"AnimRecover","AnimRecoverTransition","AnimRecoverCat");
                number(value.speed,"AnimSpeedFactor");number(value.speed_transition,"AnimSpeedTransitTime");number(value.speed_range,"AnimSpeedTransitTimeRange");
                number(value.repeat_occur_at,"RepeatOccurAnimWhenBeHitNormalizedTime");number(value.material_transition,"MaterialTransitTime");
                number(value.floating_height,"FloatingHeight");number(value.floating_rise,"FloatingRiseDuration");number(value.floating_fall,"FloatingFallDuration");
                boolean(value.force_hit_h,"ForecHitH");boolean(value.disable_look_at,"DisableLookAtIK");
                boolean(value.forbid_special_standby,"SpecialStandbyForbidVisualState");boolean(value.forbid_special_character,"SpecialCharacterForbidVisualState");
                boolean(value.only_player,"OnlyAffectPlayer");boolean(value.ignore_anim_config,"IgnoreAnimConfig");
                void* reaction=nullptr;bool reaction_read=false;
                try {
                    reaction=Read<void*>(row,Field(fields,"ReactionAnims","RPG.GameCore.CharacterReactionAnimConfig"));reaction_read=true;
                    if(reaction) {
                        value.reactions.present=true;const auto declared=metadata.ReadObjectFields(reaction);
                        auto text_field=[&](TextInput& out,const char* name){capture(out,name,[&]{return String(reaction,declared,name);});};
                        auto& r=value.reactions;text_field(r.hit,"Hit");text_field(r.hit_h,"HitH");text_field(r.hit_fly,"HitFly");text_field(r.hit_fly_recover,"HitFlyRecover");
                        text_field(r.fly_hit,"FlyHit");text_field(r.die,"Die");text_field(r.die_hit,"DieHit");text_field(r.die_fly,"DieFly");text_field(r.die_limbo,"DieLimbo");
                        capture(r.force_hit_h,"ReactionAnims.ForceHitH",[&]{return Read<std::uint8_t>(reaction,Field(declared,"ForceHitH","System.Boolean"))!=0;});
                        capture(r.hit_weight,"ReactionAnims.OverrideHitAnimWeight",[&]{return Read<float>(reaction,Field(declared,"OverrideHitAnimWeight","System.Single"));});
                    }
                }catch(const std::exception& e){value.issues.push_back({index,"ReactionAnims",e.what()});}
                auto resolve=[&](VisualPhaseInput& part,const char* label) {
                    if(part.category.available && part.category.value==0){part.resolved_state=part.state;return;}
                    capture(part.resolved_state,label,[&] {
                        Require(part.category.available && reaction_read,"category/reaction source unread");
                        const auto category=part.category.value;
                        Scope query(*this);
                        auto read_name=[&](ObjectResult result) {
                            Require(result.call.returned,"official category query failed");
                            return result.object?Text(query.Keep(result)):std::string();
                        };
                        auto lookup=methods.Resolve("",detail::catalog_bindings::animation_names,detail::catalog_bindings::reaction_name,"System.String",{"RPG.GameCore.AnimCategory","RPG.GameCore.CharacterReactionAnimConfig"},MethodKind::Static);
                        auto name=read_name(Call(lookup,category,reaction));
                        if(name.empty() && phase_config)name=read_name(Call(lookup,category,phase_config));
                        if(name.empty()) {
                            const auto fallback=methods.Resolve("",detail::catalog_bindings::animation_names,detail::catalog_bindings::default_reaction_name,"System.String",{"RPG.GameCore.AnimCategory"},MethodKind::Static);
                            name=read_name(Call(fallback,category));
                        }
                        return name;
                    });
                };
                resolve(value.occur,"AnimOccur.resolved");resolve(value.loop,"AnimLoop.resolved");resolve(value.recover,"AnimRecover.resolved");
                return value;
            });
    }
VisualConfigInput OfficialCatalog::ReadDefaultVisualConfig() {
    VisualConfigInput output;output.source="RPG.GameCore.GameCoreConfigManager.GameCoreConstValueConfig.ModifierBehaviorVisualList";
    auto& s=*state_;State::Scope scope(s);
    try {
        auto* config=s.GlobalVisualConfig(scope);
        output.loaded=true;output.behaviors=s.VisualRows(config,"ModifierBehaviorVisualList");
    }catch(const std::exception& e){output.issues.push_back({-1,"default visual config",e.what()});}
    return output;
}
VisualConfigInput OfficialCatalog::ReadCharacterVisualConfig(const std::string& path) {
    VisualConfigInput output;output.source=path+".PhaseAnimConfig.ModifierBehaviorVisuals";
    auto& s=*state_;State::Scope scope(s);
    try {
        auto* config=s.Config("CharacterConfig",path,scope);
        auto* phase=Read<void*>(config,Field(s.metadata.ReadObjectFields(config),"PhaseAnimConfig","RPG.GameCore.CharacterPhaseAnimConfig"));
        output.loaded=true;output.phase_present=phase!=nullptr;
        if(phase)output.behaviors=s.VisualRows(phase,"ModifierBehaviorVisuals",phase);
        else output.behaviors.enumerated=true;
    }catch(const std::exception& e){output.issues.push_back({-1,"character visual config",e.what()});}
    return output;
}
VisualConfigSources OfficialCatalog::State::VisualSources(const std::string& path,void* phase,bool available,
    const std::string& phase_source) {
    VisualConfigSources output;output.character_config=path;
    // 4.6 module 6AAAE554/20FD9000: CA154CA passes -1; CA10A50..A6B
    // selects float 0.2 at RVA40E6008. This is a versioned adapter fact, not
    // inferred from AnimOccur/Loop/RecoverTransition or a per-character map.
    output.expression_transition={true,detail::catalog_bindings::expression_transition,{}};
    // CFF PGEEGFMCBAM equals the observed Emo_Stun short hash; its entry/exit
    // call PEPN's source 3. Current enum observation names that source Stun.
    output.blink_suppressed_expression={true,detail::catalog_bindings::blink_expression,{}};
    output.blink_source={true,detail::catalog_bindings::blink_source,{}};
    output.defaults.source="RPG.GameCore.GameCoreConfigManager.GameCoreConstValueConfig.ModifierBehaviorVisualList";
    output.character.source=phase_source+".ModifierBehaviorVisuals";
    auto& s=*this;Scope scope(s);
    output.context_available=available;output.character.loaded=available;
    output.character.phase_present=available && phase!=nullptr;
    if(output.context_available) {
        try {
            if(phase)output.character.behaviors=s.VisualRows(phase,"ModifierBehaviorVisuals",phase);
            else output.character.behaviors.enumerated=true;
        }catch(const std::exception& e){output.character.issues.push_back({-1,"character visual config",e.what()});}
    }
    // Retain independent defaults on a failed context read for investigation,
    // but do not label those results as character-resolved.
    try {
        auto* config=s.GlobalVisualConfig(scope);output.defaults.loaded=true;
        output.defaults.behaviors=s.VisualRows(config,"ModifierBehaviorVisualList",phase);
    }catch(const std::exception& e){output.defaults.issues.push_back({-1,"default visual config",e.what()});}
    return output;
}
VisualConfigSources OfficialCatalog::ReadVisualConfigSources(const std::string& path) {
    auto& s=*state_;State::Scope scope(s);void* phase=nullptr;std::string error;
    try {
        auto* config=s.Config("CharacterConfig",path,scope);
        phase=Read<void*>(config,Field(s.metadata.ReadObjectFields(config),"PhaseAnimConfig","RPG.GameCore.CharacterPhaseAnimConfig"));
    }catch(const std::exception& e){error=e.what();}
    auto output=s.VisualSources(path,phase,error.empty(),path+".PhaseAnimConfig");
    if(!error.empty())output.character.issues.push_back({-1,"character phase context",error});
    return output;
}
}
