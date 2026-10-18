#include "CatalogReader.hpp"

namespace hksr::sdk_v1 {
ActionConfigInput OfficialCatalog::ReadActionConfig(ActionConfigKind kind,const std::string& path) {
    return ReadActionConfigDetail(kind,path,nullptr);
}
TaskConfigInput OfficialCatalog::ReadTaskConfig(const std::string& path) {
    TaskConfigInput result;
    result.actions=ReadActionConfigDetail(ActionConfigKind::Battle,path,&result);
    return result;
}
ActionConfigInput OfficialCatalog::ReadActionConfigDetail(ActionConfigKind kind,const std::string& path,TaskConfigInput* tasks) {
    ActionConfigInput output; output.kind=kind; output.path=path; output.presentation.path=path;
    auto& s=*state_; State::Scope scope(s);
    auto capture=[&](auto& destination,const std::string& location,auto read) {
        try { destination.value=read(); destination.available=true; }
        catch(const std::exception& error) { destination.error=error.what(); output.issues.push_back({-1,location,error.what()}); }
    };
    auto text=[&](void* object,const auto& fields,const char* name,const std::string& prefix) {
        TextInput value; capture(value,prefix+name,[&] { return s.String(object,fields,name); }); return value;
    };
    auto reference=[](auto& list,const TextInput& input,const std::string& field) {
        if(input.available && !input.value.empty()) list.push_back({input.value,field});
    };
    auto collect_issues=[&](const auto& batch,const std::string& prefix) {
        for(const auto& issue:batch.issues) output.issues.push_back({issue.index,prefix+issue.operation,issue.error});
    };
    auto task_capture=[&](auto& destination,int index,const std::string& location,auto read) {
        try { destination.value=read(); destination.available=true; }
        catch(const std::exception& error) { destination.error=error.what(); tasks->issues.push_back({index,location,error.what()}); }
    };
    auto source=[&](void* object,const auto& fields,int index,const char* name,const char* type,bool array=false) {
        SkillObjectSource value;value.field="SkillList["+std::to_string(index)+"]."+name;value.declared_type=type;
        void* reference=nullptr;
        task_capture(value.present,index,value.field,[&]{reference=Read<void*>(object,Field(fields,name,type));return reference!=nullptr;});
        if(array && value.present.available && reference)task_capture(value.element_count,index,value.field,[&]{
            const auto size=s.arrays.Length(reference);Require(size.read.returned && size.value>=0,"condition array length unavailable");return size.value;
        });
        return value;
    };
    try {
        const char* type=nullptr;
        switch(kind) {
        case ActionConfigKind::Adventure: type="AdventureCharacterConfig"; break;
        case ActionConfigKind::Battle: type="CharacterConfig"; break;
        case ActionConfigKind::AnimationEvent: type="CharacterAnimEventConfig"; break;
        case ActionConfigKind::Elation: type="ElationConfigList"; break;
        default: throw std::runtime_error("unknown action configuration kind");
        }
        auto config=s.Config(type,path,scope); output.loaded=true;
        const auto fields=s.metadata.ReadObjectFields(config);
        if(kind==ActionConfigKind::Elation) {
            output.elation_ready=s.Rows<ElationReadyInput>(config,fields,"ConfigList",[&](void* row,const auto& declared,int index) {
                ElationReadyInput value;value.source_index=index;const auto prefix="ConfigList["+std::to_string(index)+"].";
                value.skill=text(row,declared,"SkillName",prefix);
                value.entry=text(row,declared,"ReadyAnim",prefix);
                value.loop=text(row,declared,"ReadyLoopAnim",prefix);
                reference(output.states,value.entry,prefix+"ReadyAnim");
                reference(output.states,value.loop,prefix+"ReadyLoopAnim");return value;
            });
            collect_issues(output.elation_ready,"");return output;
        }
        if(kind==ActionConfigKind::AnimationEvent) {
            output.referenced_event_file=text(config,fields,"ReferenceJsonConfig","");
            // Event records may be value types. Array.GetValue / ICollection
            // CopyTo boxes them; never treat get_Item<T> as reference-returning.
            auto named_states=[&](void* object,const auto& declared,const char* name,const std::string& prefix) {
                auto rows=s.Rows<TextInput>(object,declared,name,[&](void* row,const auto& row_fields,int index) {
                    const auto location=prefix+name+"["+std::to_string(index)+"].";
                    auto value=text(row,row_fields,"Name",location);
                    reference(output.states,value,location+"Name"); return value;
                }); collect_issues(rows,prefix);
            };
            auto events=s.Rows<TextInput>(config,fields,"AnimatorStateEvents",[&](void* row,const auto& row_fields,int index) {
                const auto prefix="AnimatorStateEvents["+std::to_string(index)+"].";
                auto value=text(row,row_fields,"AnimatorStateName",prefix);
                reference(output.states,value,prefix+"AnimatorStateName");
                named_states(row,row_fields,"AnimatorStates",prefix); return value;
            }); collect_issues(events,"");
            auto groups=s.Rows<int>(config,fields,"AnimatorStateGroupEvents",[&](void* row,const auto& row_fields,int index) {
                named_states(row,row_fields,"States","AnimatorStateGroupEvents["+std::to_string(index)+"]."); return index;
            }); collect_issues(groups,"");
            return output;
        }
        output.presentation=s.Presentation(config,fields,path);
        reference(output.states,output.presentation.initial_state,"InitAnimStateName");
        if(kind==ActionConfigKind::Adventure) return output;
        output.elation_config=text(config,fields,"ElationConfigPath","");
        output.skills=s.Rows<SkillInput>(config,fields,"SkillList",[&](void* row,const auto& row_fields,int index) {
            SkillInput skill; skill.source_index=index; const auto prefix="SkillList["+std::to_string(index)+"].";
            skill.name=text(row,row_fields,"Name",prefix);
            skill.ready_state=text(row,row_fields,"ReadyAnimState",prefix);
            skill.entry_ability=text(row,row_fields,"EntryAbility",prefix);
            skill.prepare_ability=text(row,row_fields,"PrepareAbility",prefix);
            skill.ready_effect=text(row,row_fields,"ReadyEffectPath",prefix);
            reference(output.states,skill.ready_state,prefix+"ReadyAnimState");
            reference(output.abilities,skill.entry_ability,prefix+"EntryAbility");
            reference(output.abilities,skill.prepare_ability,prefix+"PrepareAbility");
            capture(skill.type,prefix+"SkillType",[&] { return Read<int>(row,Field(row_fields,"SkillType","RPG.GameCore.SkillType")); });
            auto boolean=[&](ConfigValue<bool>& target,const char* name) {
                capture(target,prefix+name,[&] { return Read<unsigned char>(row,Field(row_fields,name,"System.Boolean"))!=0; });
            };
            boolean(skill.use_transition,"UseReadyTransitAnim"); boolean(skill.cut_1_to_2,"Skill1To2AlwaysCut");
            boolean(skill.cut_2_to_1,"Skill2To1AlwaysCut"); boolean(skill.wait_prepare,"WaitPrepareFinish");
            boolean(skill.auto_standby,"AutoStandbyOnSkillFinish");
            capture(skill.ready_wait,prefix+"ReadyAnimWaitTime",[&] { return Read<float>(row,Field(row_fields,"ReadyAnimWaitTime","System.Single")); });
            skill.child_skills=s.Strings(row,row_fields,"ChildSkillList");
            if(!skill.child_skills.available) output.issues.push_back({index,prefix+"ChildSkillList",skill.child_skills.error});
            skill.variants=s.Rows<SkillReadyVariant>(row,row_fields,"CustomReadyConfigs",[&](void* variant,const auto& variant_fields,int vi) {
                SkillReadyVariant value; value.source_index=vi; const auto vp=prefix+"CustomReadyConfigs["+std::to_string(vi)+"].";
                value.state=text(variant,variant_fields,"ReadyAnimState",vp); reference(output.states,value.state,vp+"ReadyAnimState");
                capture(value.has_condition,vp+"Condition",[&] { return Read<void*>(variant,Field(variant_fields,"Condition","RPG.GameCore.PredicateConfig"))!=nullptr; });
                capture(value.priority,vp+"AutoSelectPriority",[&] { return Read<int>(variant,Field(variant_fields,"AutoSelectPriority","System.Int32")); });
                return value;
            }); collect_issues(skill.variants,prefix);
            if(tasks) {
                SkillTaskInput task;task.skill=skill;
                task_capture(task.use_type,index,prefix+"UseType",[&]{return Read<int>(row,Field(row_fields,"UseType","RPG.GameCore.SkillUseType"));});
                task_capture(task.boost_skill,index,prefix+"BoostSkill",[&]{return s.String(row,row_fields,"BoostSkill");});
                task.usable_conditions=source(row,row_fields,index,"UsableConditions","RPG.GameCore.UsableConditionConfig[]",true);
                task.visible_condition=source(row,row_fields,index,"VisibleCondition","RPG.GameCore.PredicateConfig");
                task.insert_condition=source(row,row_fields,index,"InsertCondition","RPG.GameCore.PredicateConfig");
                task.target=source(row,row_fields,index,"TargetInfo","RPG.GameCore.SkillTargetConfig");
                tasks->skills.push_back(std::move(task));
            }
            return skill;
        }); collect_issues(output.skills,"");
        output.transitions=s.Rows<ReadyTransitionInput>(config,fields,"SkillReadyTransits",[&](void* row,const auto& row_fields,int index) {
            ReadyTransitionInput value; value.source_index=index; const auto prefix="SkillReadyTransits["+std::to_string(index)+"].";
            value.state=text(row,row_fields,"AnimStateName",prefix); reference(output.states,value.state,prefix+"AnimStateName");
            capture(value.from,prefix+"From",[&] { return Read<int>(row,Field(row_fields,"From","System.Int32")); });
            capture(value.to,prefix+"To",[&] { return Read<int>(row,Field(row_fields,"To","System.Int32")); });
            capture(value.transition_in,prefix+"TransitionInTime",[&] { return Read<float>(row,Field(row_fields,"TransitionInTime","System.Single")); });
            capture(value.reverse_outro,prefix+"ReverseOutroTime",[&] { return Read<float>(row,Field(row_fields,"ReverseOutroTime","System.Single")); });
            return value;
        }); collect_issues(output.transitions,"");
        auto groups=s.Rows<SkillAbilityInput>(config,fields,"SkillAbilityList",[&](void* row,const auto& row_fields,int index) {
            SkillAbilityInput group;group.source_index=index;
            group.abilities=s.Strings(row,row_fields,"AbilityList");const auto& values=group.abilities;
            const auto prefix="SkillAbilityList["+std::to_string(index)+"].AbilityList";
            // Strings currently returns an all-or-error snapshot; never turn a
            // failed array read into a claim that this skill has no abilities.
            if(!values.available) output.issues.push_back({index,prefix,values.error});
            else for(std::size_t i=0;i<values.values.size();++i) if(!values.values[i].empty())
                output.abilities.push_back({values.values[i],prefix+"["+std::to_string(i)+"]"});
            if(tasks)task_capture(group.skill,index,"SkillAbilityList["+std::to_string(index)+"].Skill",[&]{return s.String(row,row_fields,"Skill");});
            return group;
        }); collect_issues(groups,"");
        if(tasks)tasks->skill_abilities=std::move(groups);
    } catch(const std::exception& error) { output.issues.push_back({-1,"action configuration",error.what()}); }
    return output;
}
}
