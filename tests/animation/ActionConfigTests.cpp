#include <hksr/animation/OfficialCatalog.hpp>
#include <array>
#include <map>
#include <memory>
#include <cstring>
#include <stdexcept>

namespace {
using namespace hksr::api;
void CheckAt(bool yes,int line) { if(!yes) throw std::runtime_error("action config test failed at line "+std::to_string(line)); }
#define Check(...) CheckAt((__VA_ARGS__),__LINE__)
struct Node {
    std::array<std::uint64_t,48> slots{};
    std::vector<DeclaredField> fields;
    std::vector<void*> items;
    std::wstring text;
    std::string type="FixtureConfig";
};
std::vector<std::unique_ptr<Node>> nodes;
std::map<std::wstring,Node*> documents;
std::map<int,void*> roots;
int metadata,handle=0,loads=0;
Node* visual_global=nullptr;
Node* reflected_type=nullptr;
Node* reflected_field=nullptr;
Node* helper_type=nullptr;
std::map<std::wstring,Node*> helper_values;
std::map<Node*,std::wstring> category_overrides;
std::map<Node*,Node*> clone_sources;
bool fail_visual_allocation=false,alias_visual_clone=false;
bool ready_query_unavailable=false;
int prepared_rows=0;
Node* New() { nodes.push_back(std::make_unique<Node>()); return nodes.back().get(); }
Node* Text(const wchar_t* value) { auto node=New(); node->text=value; return node; }
Node* Array(std::initializer_list<void*> values) { auto node=New(); node->items=values; return node; }
template<class T> void Field(Node* node,const char* name,const char* type,T value) {
    const auto index=node->fields.size(); Check(index<node->slots.size() && sizeof(T)<=8);
    std::memcpy(&node->slots[index],&value,sizeof(T));
    node->fields.push_back({"RPG.GameCore","FixtureConfig",name,type,offsetof(Node,slots)+index*8,0,true});
}
void String(Node* node,const char* field,const wchar_t* value) { Field(node,field,"System.String",Text(value)); }
void Sequence(Node* node,const char* field,Node* value) { Field(node,field,"RPG.GameCore.FixtureRow[]",value); }
void Strings(Node* node,const char* field,Node* value) { Field(node,field,"System.String[]",value); }
template<class T> Method M(T f) { return {reinterpret_cast<void*>(f),&metadata}; }
int __fastcall Retain(void* object,int prior,int kind,void* info) { Check(!prior && kind==2 && info==&metadata); roots[++handle]=object; return handle; }
void* __fastcall Target(int id,void*) { return roots.at(id); }
void __fastcall Free(int id,void*) { Check(roots.erase(id)==1); }
void* __fastcall Load(Node* key,void*) { ++loads; auto found=documents.find(key->text); return found==documents.end()?nullptr:found->second; }
PhaseConfigRoot last_definition_root=PhaseConfigRoot::GlobalModifier;
void* __fastcall LoadAbility(Node* key,void* info){last_definition_root=PhaseConfigRoot::Ability;return Load(key,info);}
void* __fastcall LoadModifier(Node* key,void* info){last_definition_root=PhaseConfigRoot::GlobalModifier;return Load(key,info);}
void* __fastcall LoadTemplate(Node* key,void* info){last_definition_root=PhaseConfigRoot::GlobalTemplate;return Load(key,info);}
void* __fastcall ReflectField(Node* type,Node* name,int flags,void* info) {
    Check(flags==56 && info==&metadata);
    if(type==helper_type) {auto field=New();field->type="StaticField";field->text=name->text;return field;}
    Check(type==reflected_type && name->text==L"GameCoreConstValueConfig");
    return reflected_field;
}
void* __fastcall ReflectValue(Node* field,void* instance,void* info) {
    Check(!instance && info==&metadata);
    if(field->type=="StaticField")return helper_values.at(field->text);
    Check(field==reflected_field);return visual_global;
}
void* __fastcall CloneVisual(Node* source,void* info) {
    Check(info==&metadata);
    if(alias_visual_clone)return source;
    auto copy=New();*copy=*source;clone_sources[copy]=source;return copy;
}
void __fastcall VisualTableCtor(Node* table,void* info) {
    Check(info==&metadata && table->type=="PrivateVisualTable");table->items.clear();
}
void __fastcall PrepareVisual(Node* table,Node* row,int flag,int index,void* info) {
    Check(info==&metadata && table->type=="PrivateVisualTable" && clone_sources.count(row) && index>=0);
    int actual=0;for(const auto& f:row->fields)if(f.name=="Flag")std::memcpy(&actual,reinterpret_cast<char*>(row)+f.offset,4);
    Check(flag==actual);++prepared_rows;
    // Fake the dependency's contract, not its implementation. Deliberately
    // derive from strings rather than accepting possibly cold/stale Cat fields.
    for(const auto& pair:std::array<std::pair<const char*,const char*>,3>{{
        {"AnimOccur","AnimOccurCat"},{"AnimLoop","AnimLoopCat"},{"AnimRecover","AnimRecoverCat"}}}) {
        Node* name=nullptr;
        for(const auto& f:row->fields)if(f.name==pair.first)std::memcpy(&name,reinterpret_cast<char*>(row)+f.offset,sizeof(name));
        std::uint8_t category=name && name->text==L"Hit_Fly_Recover"?8:0;
        for(const auto& f:row->fields)if(f.name==pair.second)std::memcpy(reinterpret_cast<char*>(row)+f.offset,&category,1);
    }
    table->items.push_back(row);
}
void* __fastcall CategoryName(std::uint8_t category,Node* config,void* info) {
    Check(category==8 && info==&metadata);const auto found=category_overrides.find(config);
    return found==category_overrides.end()?nullptr:Text(found->second.c_str());
}
void* __fastcall DefaultCategoryName(std::uint8_t category,void* info) {
    Check(info==&metadata);
    if(category==1)return Text(L"Fixture_Hit");
    if(category==2)return Text(L"Fixture_Heavy");
    if(category==7)return Text(L"Fixture_Launch");
    if(category==9)return Text(L"Fixture_AirHit");
    Check(category==8);return Text(L"Official_Default_Category");
}
void* __fastcall DefaultReadyName(std::int32_t from,std::int32_t to,void* info) {
    Check(info==&metadata);
    if(from==-1 && to==0)return nullptr;
    if(from>=0 && from<3 && to>=0 && to<3 && from!=to)return Text(L"Fixture_Fallback");
    Check(from==8 && to==11);return Text(L"Fixture_Ready_Transition");
}
int __fastcall Count(Node* node,void*) { return static_cast<int>(node->items.size()); }
void* __fastcall Item(Node* node,int index,void*) { return node->items.at(index); }
void* __fastcall CopyArray(void*,int size,void*) { auto node=New(); node->items.resize(size); return node; }
void __fastcall Copy(Node* node,Node* to,int index,void*) { Check(index==0); to->items=node->items; }
void* __fastcall Keys(Node* node,void*) { return node->items.at(0); }
void* __fastcall Value(Node* node,Node* key,void*) {
    Check(key->text==L"branch_key"); return node->items.at(1);
}
int __fastcall Length(Node* node,void*) { return static_cast<int>(node->text.size()); }
std::uint16_t __fastcall Char(Node* node,int index,void*) { return static_cast<std::uint16_t>(node->text.at(index)); }
struct Inputs:MethodResolver,ManagedObjects,ObjectMetadata,TypeRelationships {
    int full_types=0, field_reads=0;
    RuntimeTypeName ReadObjectTypeName(void* object) override {
        return {"RPG.GameCore",static_cast<Node*>(object)->type};
    }
    Method Resolve(const char* space,const char* type,const char* name,const char* result,
        std::initializer_list<const char*> parameters,MethodKind kind) override {
        if(std::string(space)=="System") {
            if(std::string(type)=="Object") {
                Check(std::string(name)=="MemberwiseClone" && std::string(result)=="System.Object" && parameters.size()==0 && kind==MethodKind::Instance);
                return M(CloneVisual);
            }
            Check(std::string(type)=="RuntimeType" && std::string(name)=="GetField" && std::string(result)=="System.Reflection.FieldInfo" && kind==MethodKind::Instance);
            Check(std::vector<std::string>(parameters.begin(),parameters.end())==std::vector<std::string>{"System.String","System.Reflection.BindingFlags"});return M(ReflectField);
        }
        if(std::string(space).empty()) {
            if(std::string(type)=="LCOMIBOIAPN") {
                Check(std::string(result)=="System.Void" && kind==MethodKind::Instance);
                if(std::string(name)==".ctor"){Check(parameters.size()==0);return M(VisualTableCtor);}
                Check(std::string(name)=="PEJCNLBKIGN" && std::vector<std::string>(parameters.begin(),parameters.end())==
                    std::vector<std::string>{"RPG.GameCore.ModifierBehaviorVisual","System.Int32","System.Int32"});return M(PrepareVisual);
            }
            Check(std::string(type)=="NNMPPHHHEIK" && std::string(result)=="System.String" && kind==MethodKind::Static);
            if(std::string(name)=="AODEJIKGNFJ") {
                Check(std::vector<std::string>(parameters.begin(),parameters.end())==std::vector<std::string>{"System.Int32","System.Int32"});
                if(ready_query_unavailable)throw std::runtime_error("ready query unavailable");
                return M(DefaultReadyName);
            }
            if(std::string(name)=="ECACKGEBFGC") {
                Check(std::vector<std::string>(parameters.begin(),parameters.end())==std::vector<std::string>{"RPG.GameCore.AnimCategory","RPG.GameCore.CharacterReactionAnimConfig"});return M(CategoryName);
            }
            Check(std::string(name)=="PJKNFGDJCAK" && parameters.size()==1 && std::string(*parameters.begin())=="RPG.GameCore.AnimCategory");return M(DefaultCategoryName);
        }
        Check(std::string(space)=="RPG.GameCore" && kind==MethodKind::Static);
        if(std::string(type)=="AbilityJsonTable") {
            Check(((std::string(name)=="GetTurnBasedAbility" && std::string(result)=="RPG.GameCore.TurnBasedAbilityConfig") ||
                (std::string(name)=="GetGlobalModifierConfig" && std::string(result)=="RPG.GameCore.TurnBasedModifierConfig") ||
                (std::string(name)=="GetGlobalTaskListTemplate" && std::string(result)=="RPG.GameCore.TaskListTemplate")) &&
                parameters.size()==1 && std::string(*parameters.begin())=="System.String");
            if(std::string(name)=="GetTurnBasedAbility")return M(LoadAbility);
            if(std::string(name)=="GetGlobalModifierConfig")return M(LoadModifier);
            return M(LoadTemplate);
        }
        Check(std::string(type)=="GameCoreConfigLoader");
        Check(parameters.size()==1 && std::string(*parameters.begin())=="System.String");
        const std::string n=name,r=result;
        Check((n=="LoadAdventureCharacterConfig" && r=="RPG.GameCore.AdventureCharacterConfig") ||
            (n=="LoadCharacterConfig" && r=="RPG.GameCore.CharacterConfig") ||
            (n=="LoadElationConfigList" && r=="RPG.GameCore.ElationConfigList") ||
            (n=="LoadCharacterAnimEventConfig" && r=="RPG.GameCore.CharacterAnimEventConfig")); return M(Load);
    }
    ObjectResult NewStringUtf8(const char* value) override { auto node=New(); while(*value) node->text.push_back(*value++); return {{true,0},node}; }
    ObjectResult AllocateObject(const char* space,const char* name) override {
        Check(std::string(space).empty() && std::string(name)=="LCOMIBOIAPN"); // no task/actor construction
        if(fail_visual_allocation)return {};
        auto table=New();table->type="PrivateVisualTable";return {{true,0},table};
    }
    ObjectResult ManagedType(const char* space,const char* name) override {
        if(std::string(space).empty() && std::string(name)=="NNMPPHHHEIK")return {{true,0},helper_type};
        if(std::string(space)=="RPG.GameCore") {Check(std::string(name)=="GameCoreConfigManager");return {{true,0},reflected_type};}
        Check(std::string(space)=="System" && std::string(name)=="Object"); return {{true,0},New()};
    }
    DeclaredType ReadObjectType(void* object) override {
        ++full_types;
        auto node=static_cast<Node*>(object); DeclaredType type{"RPG.GameCore",node->type,{}};
        if(node==reflected_field || node->type=="StaticField")type.methods.push_back({"GetValue","System.Object",{"System.Object"},false,M(ReflectValue)});
        if(node->type=="Dictionary") {
            type.methods.push_back({"get_Keys","Test.Keys",{},false,M(Keys)});
            type.methods.push_back({"get_Item","RPG.GameCore.FixtureConfig",{"System.String"},false,M(Value)});
        }
        if(node->type=="Collection") {
            type.methods.push_back({"get_Count","System.Int32",{},false,M(Count)});
            type.methods.push_back({"get_Item","RPG.GameCore.FixtureConfig",{"System.Int32"},false,M(Item)});
            type.methods.push_back({"System.Collections.ICollection.CopyTo","System.Void",{"System.Array","System.Int32"},false,M(Copy)});
        }
        return type;
    }
    std::vector<DeclaredField> ReadObjectFields(void* object) override { ++field_reads;return static_cast<Node*>(object)->fields; }
    bool TypeDerivesFrom(const char* space,const char* name,const char* base_space,const char* base_name) override {
        if(std::string(base_space)=="System" && std::string(base_name)=="ValueType")
            return std::string(space)=="System" && std::string(name)=="Int32";
        Check(std::string(base_space)=="RPG.GameCore" && std::string(base_name)=="JsonConfig");
        return std::string(space)=="RPG.GameCore" && (std::string(name)=="FixtureConfig" || std::string(name)=="FixtureRow" ||
            std::string(name)=="TriggerAnimState" || std::string(name)=="TriggerAnimStateWithMove" || std::string(name)=="MoveToTargetList" ||
            std::string(name)=="WaitAnimState" || std::string(name)=="RemoveEffect" || std::string(name)=="TriggerAbility" ||
            std::string(name)=="SetControlSkillMapping" || std::string(name)=="UseSkillOneMore" || std::string(name)=="TargetAlias" || std::string(name)=="TargetEvaluator");
    }
};
}
void TestActionConfig() {
    nodes.clear(); documents.clear(); roots.clear(); handle=loads=prepared_rows=0;
    clone_sources.clear();fail_visual_allocation=alias_visual_clone=false;
    auto config=New(); documents[L"battle"]=config; documents[L"player"]=config;
    String(config,"InitAnimStateName",L"Default_NotHardcoded");
    String(config,"ElationConfigPath",L"elation");
    Strings(config,"AnimEventConfigList",Array({Text(L"eventA"),Text(L"eventB")}));
    Strings(config,"DisableAnimEventLayers",nullptr);
    auto variant=New(); String(variant,"ReadyAnimState",L"Conditional_Ready");
    Field(variant,"Condition","RPG.GameCore.PredicateConfig",New()); Field(variant,"AutoSelectPriority","System.Int32",7);
    auto skill=New(); String(skill,"Name",L"FollowUpCustom"); String(skill,"ReadyAnimState",L"Ready_Loop");
    String(skill,"EntryAbility",L"Ability_Entry"); String(skill,"PrepareAbility",L"Ability_Prepare");
    String(skill,"ReadyEffectPath",L"Effects/ReadyFixture");
    Field(skill,"SkillType","RPG.GameCore.SkillType",19);
    Field(skill,"UseReadyTransitAnim","System.Boolean",static_cast<unsigned char>(1));
    Field(skill,"Skill1To2AlwaysCut","System.Boolean",static_cast<unsigned char>(0));
    Field(skill,"Skill2To1AlwaysCut","System.Boolean",static_cast<unsigned char>(0));
    Field(skill,"WaitPrepareFinish","System.Boolean",static_cast<unsigned char>(1));
    Field(skill,"AutoStandbyOnSkillFinish","System.Boolean",static_cast<unsigned char>(1));
    // Deliberately absent timing: preserve the rest, don't drop this whole skill.
    Strings(skill,"ChildSkillList",Array({Text(L"Child_A")})); Sequence(skill,"CustomReadyConfigs",Array({nullptr,variant}));
    Sequence(config,"SkillList",Array({nullptr,skill}));
    auto transition=New(); String(transition,"AnimStateName",L"Ready_A_To_B");
    Field(transition,"From","System.Int32",19); Field(transition,"To","System.Int32",27);
    Field(transition,"TransitionInTime","System.Single",0.125f); Field(transition,"ReverseOutroTime","System.Single",0.5f);
    Sequence(config,"SkillReadyTransits",Array({transition}));
    auto group=New(); Strings(group,"AbilityList",Array({Text(L"Ability_Entry"),Text(L"Ability_Other")}));
    Sequence(config,"SkillAbilityList",Array({group}));
    auto event=New(); documents[L"eventA"]=event;
    String(event,"ReferenceJsonConfig",L"eventB");
    auto record=New(); String(record,"AnimatorStateName",L"Idle_Show_Custom");
    auto named=New(); String(named,"Name",L"Idle_Show_Custom");
    Sequence(record,"AnimatorStates",Array({named}));
    auto bad_record=New(); Sequence(bad_record,"AnimatorStates",Array({named}));
    Sequence(event,"AnimatorStateEvents",Array({record,bad_record}));
    auto named_group=New(); auto last=New(); String(last,"Name",L"结束状态"); Sequence(named_group,"States",Array({last}));
    Sequence(event,"AnimatorStateGroupEvents",Array({named_group}));
    Inputs input; ComponentConfigMethods text; text.string_length=M(Length); text.string_character=M(Char);
    OfficialCatalog catalog(input,input,input,{M(Retain),M(Target),M(Free),{},{}},ManagedArrays({M(CopyArray),M(Count),M(Item),{}, {}}),ComponentConfig(text));
    const auto battle=catalog.ReadActionConfig(ActionConfigKind::Battle,"battle");
    Check(loads==1 && roots.empty() && battle.loaded && battle.skills.enumerated && battle.skills.declared_count==2 && battle.skills.rows.size()==1);
    const auto& selected=battle.skills.rows[0];
    Check(selected.source_index==1 && selected.type.value==19 && !selected.ready_wait.available && selected.auto_standby.value);
    Check(selected.ready_effect.available && selected.ready_effect.value=="Effects/ReadyFixture");
    Check(selected.variants.rows[0].source_index==1 && selected.variants.rows[0].has_condition.available && selected.variants.rows[0].has_condition.value);
    Check(selected.variants.rows[0].priority.value==7 && battle.transitions.rows[0].from.value==19 && battle.transitions.rows[0].to.value==27);
    Check(battle.transitions.rows[0].transition_in.value==0.125f && battle.states.size()==4 && battle.abilities.size()==4 && battle.issues.size()==1);
    Check(battle.states[2].field=="SkillList[1].CustomReadyConfigs[1].ReadyAnimState");
    Check(battle.abilities[0].value=="Ability_Entry" && battle.abilities[2].value=="Ability_Entry"); // retain both provenances
    auto adventure=catalog.ReadActionConfig(ActionConfigKind::Adventure,"player");
    Check(loads==2 && roots.empty() && adventure.loaded && adventure.states.size()==1 && adventure.skills.rows.empty() && adventure.abilities.empty());
    auto events=catalog.ReadActionConfig(ActionConfigKind::AnimationEvent,"eventA");
    Check(loads==3 && roots.empty() && events.loaded && events.referenced_event_file.value=="eventB" && events.states.size()==4 && events.issues.size()==1);
    Check(events.states[2].field=="AnimatorStateEvents[1].AnimatorStates[0].Name" && events.states[3].value==u8"结束状态");
    const auto absent=catalog.ReadActionConfig(ActionConfigKind::AnimationEvent,"missing");
    Check(!absent.loaded && absent.issues.size()==1 && roots.empty());
    Check(battle.elation_config.available && battle.elation_config.value=="elation");
    auto elation=New();documents[L"elation"]=elation;
    auto elation_row=New();String(elation_row,"SkillName",L"AnySkill");String(elation_row,"ReadyAnim",L"AnyEntry");
    String(elation_row,"ReadyLoopAnim",L"AnyLoop");Sequence(elation,"ConfigList",Array({nullptr,elation_row}));
    const auto extra=catalog.ReadActionConfig(ActionConfigKind::Elation,"elation");
    Check(extra.loaded && extra.elation_ready.enumerated && extra.elation_ready.rows.size()==1 && roots.empty());
    Check(extra.elation_ready.rows[0].source_index==1 && extra.elation_ready.rows[0].skill.value=="AnySkill" &&
        extra.elation_ready.rows[0].entry.value=="AnyEntry" && extra.elation_ready.rows[0].loop.value=="AnyLoop");
    Check(extra.states.size()==2 && extra.states[1].field=="ConfigList[1].ReadyLoopAnim");
    auto task=New(); task->type="TriggerAnimStateWithMove"; String(task,"AnimStateName",L"Skill_FollowUp_Loop");
    Field(task,"AnimLogicState","RPG.GameCore.AnimStateLogicType",23);
    Field(task,"TaskEnabled","System.Boolean",static_cast<unsigned char>(1));
    // WaitAnimState missing: preserve its state and mark the task incomplete.
    auto disabled=New(); disabled->type="TriggerAnimState"; String(disabled,"AnimStateName",L"Disabled_State");
    Field(disabled,"AnimLogicState","RPG.GameCore.AnimStateLogicType",8);
    Field(disabled,"TaskEnabled","System.Boolean",static_cast<unsigned char>(0));
    auto root=New(); Sequence(root,"Tasks",Array({task,disabled,task}));
    Field(task,"Cycle","RPG.GameCore.FixtureConfig",root);
    // Non-config value bits must never be dereferenced as a managed pointer.
    Field(root,"InlineValue","RPG.GameCore.DynamicFloat",static_cast<std::uint64_t>(1));
    Field(root,"ValueDictionary","System.Collections.Generic.Dictionary<System.String,RPG.GameCore.DynamicFloat>",static_cast<std::uint64_t>(1));
    auto keys=Array({Text(L"branch_key")}); keys->type="Collection";
    auto dict=Array({keys,disabled}); dict->type="Dictionary";
    auto root2=New(); Field(root2,"Branches","System.Collections.Generic.Dictionary<System.String,RPG.GameCore.FixtureConfig>",dict);
    Field(root2,"SharedTask","RPG.GameCore.TriggerAnimStateWithMove",task);
    documents[L"abilityA"]=root; documents[L"abilityB"]=root2;
    input.full_types=input.field_reads=0;
    const auto graph=catalog.ReadAbilitySources({"abilityA","abilityB","abilityA","missing"},input);
    Check(roots.empty() && graph.nodes_read==4 && graph.animations.size()==8 && graph.issues.size()==3);
    // Methods are needed only for the dictionary and keys, not graph nodes.
    // Repeated edges, cycles and shared nodes must not reread node fields.
    Check(input.full_types==2 && input.field_reads==4);
    Check(graph.animations[0].kind==AnimationTaskKind::StateWithMove && !graph.animations[0].waits.available);
    Check(graph.animations[0].logic_kind.value==23 && graph.animations[0].path[0].index==0);
    Check(graph.animations[1].enabled.available && !graph.animations[1].enabled.value);
    Check(graph.animations[2].path[0].index==2); // shared task's second ordered slot is preserved
    Check(graph.animations[3].path[0].selection==ConfigSelection::StringKey && graph.animations[3].path[0].key=="branch_key");
    // A zero slice advances one work unit, allowing deterministic suspension
    // at every field, array/dictionary element and release without sleeping.
    input.full_types=input.field_reads=0;
    auto incremental=catalog.BeginAbilitySources({"abilityA","abilityB","abilityA","missing"},input);
    Check(loads>0 && roots.empty() && !incremental->Done());
    bool premature=false;try{incremental->Take();}catch(const std::exception&){premature=true;}Check(premature);
    std::size_t slices=0;bool retained=false;
    while(!incremental->Done()) {
        incremental->Advance(0);++slices;retained|=!roots.empty();
        Check(slices<1000); // fixture termination assertion, not a production limit
    }
    auto sliced=incremental->Take();
    Check(slices>20 && retained && roots.empty() && input.full_types==2 && input.field_reads==4);
    Check(sliced.nodes_read==graph.nodes_read && sliced.animations.size()==graph.animations.size() && sliced.issues.size()==graph.issues.size());
    for(std::size_t i=0;i<graph.animations.size();++i) {
        const auto& a=graph.animations[i];const auto& b=sliced.animations[i];
        Check(a.ability==b.ability && a.state.value==b.state.value && a.path.size()==b.path.size());
        for(std::size_t j=0;j<a.path.size();++j)Check(a.path[j].field==b.path[j].field &&
            a.path[j].index==b.path[j].index && a.path[j].key==b.path[j].key && a.path[j].selection==b.path[j].selection);
    }
    {
    // Complete roots can be consumed before the batch/GC drain ends; shared
    // nodes still get read once, and undrained legacy Take retains all results.
    input.full_types=input.field_reads=0;
    auto streaming=catalog.BeginAbilitySources({"abilityA","abilityB","abilityA","missing"},input);
    auto* progress=dynamic_cast<ProgressiveAbilitySourceRead*>(streaming.get());Check(progress!=nullptr);
    AbilitySources combined;std::vector<std::string> delivered;bool early_delivery=false;
    while(!streaming->Done()) {
        streaming->Advance(0);auto part=progress->DrainCompleted();
        if(!part.abilities.empty())early_delivery|=!streaming->Done();
        delivered.insert(delivered.end(),part.abilities.begin(),part.abilities.end());
        combined.animations.insert(combined.animations.end(),part.sources.animations.begin(),part.sources.animations.end());
        combined.issues.insert(combined.issues.end(),part.sources.issues.begin(),part.sources.issues.end());
        combined.nodes_read+=part.sources.nodes_read;
    }
    Check(early_delivery && delivered==std::vector<std::string>({"abilityA","abilityB","missing"}));
    Check(roots.empty() && streaming->Take().animations.empty() && progress->DrainCompleted().abilities.empty());
    Check(input.full_types==2 && input.field_reads==4 && combined.nodes_read==graph.nodes_read);
    Check(combined.animations.size()==graph.animations.size() && combined.issues.size()==graph.issues.size());
    for(std::size_t i=0;i<graph.animations.size();++i) {
        Check(combined.animations[i].ability==graph.animations[i].ability && combined.animations[i].state.value==graph.animations[i].state.value);
        Check(combined.animations[i].path.size()==graph.animations[i].path.size());
    }
    auto partially_cancelled=catalog.BeginAbilitySources({"abilityA","abilityB"},input);
    auto* partial=dynamic_cast<ProgressiveAbilitySourceRead*>(partially_cancelled.get());
    CompletedAbilitySources first;
    while(first.abilities.empty()){partially_cancelled->Advance(0);first=partial->DrainCompleted();}
    partially_cancelled->Cancel();while(!partially_cancelled->Done())partially_cancelled->Advance(0);
    Check(first.abilities==std::vector<std::string>{"abilityA"} && !first.sources.animations.empty());
    Check(partial->DrainCompleted().abilities.empty() && partially_cancelled->Take().animations.empty() && roots.empty());
    }
    auto cancelled=catalog.BeginAbilitySources({"abilityA","abilityB"},input);
    cancelled->Advance(0);cancelled->Advance(0);
    Check(!roots.empty());const auto before_cancel=loads;cancelled->Cancel();
    while(!cancelled->Done())cancelled->Advance(0);
    Check(loads==before_cancel && roots.empty() && cancelled->Take().animations.empty());
    // A large single array also yields, rather than treating an entire root or
    // node as a frame-sized unit. Repeated source slots remain represented.
    auto wide=New();auto elements=Array({});elements->items.assign(300,disabled);
    Sequence(wide,"Tasks",elements);documents[L"wide"]=wide;
    auto wide_read=catalog.BeginAbilitySources({"wide"},input);slices=0;
    while(!wide_read->Done()){wide_read->Advance(0);++slices;Check(slices<10000);}
    auto wide_result=wide_read->Take();Check(slices>300 && wide_result.animations.size()==300 && roots.empty());
    // Abandoned cursors release their own references; shared state also outlives
    // the facade, while the test's metadata provider remains alive.
    std::unique_ptr<AbilitySourceRead> detached;
    {
        OfficialCatalog temporary(input,input,input,{M(Retain),M(Target),M(Free),{},{}},ManagedArrays({M(CopyArray),M(Count),M(Item),{}, {}}),ComponentConfig(text));
        detached=temporary.BeginAbilitySources({"abilityA"},input);
        detached->Advance(0);detached->Advance(0);
    }
    Check(!roots.empty());detached.reset();Check(roots.empty());
    Field(task,"WaitAnimState","System.Boolean",static_cast<unsigned char>(1));
    const auto reread=catalog.ReadAbilitySources({"abilityA"},input);
    Check(reread.animations[0].waits.available && reread.animations[0].waits.value && reread.issues.empty() && roots.empty());
    auto caster_alias=New();caster_alias->type="TargetAlias";String(caster_alias,"Alias",L"Caster");
    Field(task,"TargetType","RPG.GameCore.TargetEvaluator",caster_alias);
    auto wait=New();wait->type="WaitAnimState";String(wait,"AnimStateName",L"Skill_FollowUp_Loop");
    Field(wait,"TaskEnabled","System.Boolean",static_cast<unsigned char>(1));Field(wait,"TargetType","RPG.GameCore.TargetEvaluator",caster_alias);
    auto removal=New();removal->type="RemoveEffect";
    String(removal,"EffectPath",L"Effects/Explicit");
    for(auto name:{"EffectPathKey","UniqueEffectName","AttachPointName"})Field(removal,name,"System.String",static_cast<void*>(nullptr));
    Field(removal,"TargetType","RPG.GameCore.TargetEvaluator",caster_alias);
    for(auto name:{"TaskEnabled","IsNeedFadeOut","Unbind","ForceImmediateFadeOut"})Field(removal,name,"System.Boolean",static_cast<unsigned char>(1));
    Field(removal,"Flags","RPG.GameCore.MonoEffectFlag[]",static_cast<void*>(nullptr));
    Field(removal,"FadeOutRegionStart","System.Single",-1.f);Field(removal,"FadeOutRegionEnd","System.Single",-1.f);
    auto clean=New();Sequence(clean,"OnStart",Array({task,wait,removal,removal}));documents[L"cleanup"]=clean;
    const auto cleanup=catalog.ReadAbilitySources({"cleanup"},input);
    Check(cleanup.waits.size()==1 && cleanup.removals.size()==2 && cleanup.removals[1].path.back().index==3 && roots.empty());
    Check(cleanup.waits[0].target_alias.value=="Caster" && cleanup.removals[0].input.enabled.available &&
        cleanup.removals[0].input.enabled.value && cleanup.removals[0].input.effect_path.value=="Effects/Explicit");
    auto cleanup_read=catalog.BeginAbilitySources({"cleanup"},input);
    while(!cleanup_read->Done())cleanup_read->Advance(0);
    const auto sliced_cleanup=cleanup_read->Take();
    Check(roots.empty() && sliced_cleanup.animations.size()==cleanup.animations.size() && sliced_cleanup.waits.size()==1 &&
        sliced_cleanup.removals.size()==2 && sliced_cleanup.removals[1].path.back().index==3 && sliced_cleanup.waits[0].target_alias.value=="Caster");
    auto literal=New();literal->type="DynamicString";Field(literal,"Custom","System.Boolean",static_cast<unsigned char>(0));String(literal,"Value",L"abilityB");
    auto link=New();link->type="TriggerAbility";Field(link,"TaskEnabled","System.Boolean",static_cast<unsigned char>(1));Field(link,"AbilityName","RPG.GameCore.DynamicString",literal);
    documents[L"entry-link"]=link;
    auto referenced=catalog.ReadAbilitySources({"entry-link"},input);
    Check(referenced.references.size()==1 && referenced.references[0].target.value=="abilityB" && referenced.references[0].enabled.value && roots.empty());
    auto dynamic=New();dynamic->type="DynamicString";Field(dynamic,"Custom","System.Boolean",static_cast<unsigned char>(1));String(dynamic,"Value",L"not-a-literal");
    for(const auto& f:link->fields)if(f.name=="AbilityName")std::memcpy(reinterpret_cast<char*>(link)+f.offset,&dynamic,sizeof(dynamic));
    referenced=catalog.ReadAbilitySources({"entry-link"},input);
    Check(!referenced.references[0].target.available && !referenced.issues.empty() && roots.empty());
    auto mapping=New();mapping->type="SetControlSkillMapping";Field(mapping,"ControlSkillType","RPG.GameCore.ControlSkillType",7);
    String(mapping,"SkillTriggerKey",L"ConfiguredKey");Field(mapping,"TargetType","RPG.GameCore.TargetEvaluator",caster_alias);Field(mapping,"TaskEnabled","System.Boolean",static_cast<unsigned char>(1));
    auto continuation=New();continuation->type="UseSkillOneMore";Field(continuation,"SkillType","RPG.GameCore.ControlSkillType",7);
    Field(continuation,"ChildSkillIndex","System.Int32",-1);Field(continuation,"TaskEnabled","System.Boolean",static_cast<unsigned char>(1));
    auto setup=New();Sequence(setup,"OnStart",Array({mapping,task,continuation}));documents[L"setup"]=setup;
    auto setup_values=catalog.ReadAbilitySources({"setup"},input);
    Check(setup_values.skill_mappings.size()==1 && setup_values.skill_mappings[0].skill.value=="ConfiguredKey" && setup_values.skill_mappings[0].control.value==7);
    Check(setup_values.skill_continuations.size()==1 && setup_values.skill_continuations[0].path[0].index==2 && setup_values.skill_continuations[0].child.value==-1 && roots.empty());
    // Original named source, never a battle constructor/cache initializer.
    reflected_type=New();reflected_field=New();reflected_field->type="FixtureField";
    visual_global=New();visual_global->type="GameCoreConstValue";
    auto visual=[&](int flag,const wchar_t* emotion,std::uint8_t category=8) {
        auto v=New();Field(v,"Flag","RPG.GameCore.ModifierBehaviorFlag",flag);
        Field(v,"MaterialEffect","RPG.GameCore.CharacterMaterialEffectType",0);
        String(v,"ModifierBehaviorVisualPriority",L"SourcePriority");String(v,"EmoAnimName",emotion);
        String(v,"AnimOccur",category?L"Hit_Fly_Recover":L"Stagger");String(v,"AnimLoop",L"Stagger_Loop");String(v,"AnimRecover",category?L"Hit_Fly_Recover":L"Stagger_Recover");String(v,"FloatingFailAnimRecover",L"");
        for(auto name:{"AnimOccurTransition","AnimLoopTransition","AnimRecoverTransition","AnimSpeedFactor","AnimSpeedTransitTime","AnimSpeedTransitTimeRange",
            "RepeatOccurAnimWhenBeHitNormalizedTime","MaterialTransitTime","FloatingHeight","FloatingRiseDuration","FloatingFallDuration"})Field(v,name,"System.Single",0.15f);
        for(auto name:{"AnimOccurCat","AnimLoopCat","AnimRecoverCat"})Field(v,name,"RPG.GameCore.AnimCategory",static_cast<std::uint8_t>(0));
        for(auto name:{"ForecHitH","DisableLookAtIK","SpecialStandbyForbidVisualState","SpecialCharacterForbidVisualState","OnlyAffectPlayer","IgnoreAnimConfig"})Field(v,name,"System.Boolean",static_cast<std::uint8_t>(1));
        Field(v,"ReactionAnims","RPG.GameCore.CharacterReactionAnimConfig",static_cast<void*>(nullptr));return v;
    };
    auto monster=visual(6,L"",0),avatar=visual(28,L"Emo_Stun"),unknown=visual(147,L"OtherEmotion");
    auto partial=New();Field(partial,"Flag","RPG.GameCore.ModifierBehaviorFlag",9001);
    Field(visual_global,"ModifierBehaviorVisualList","RPG.GameCore.ModifierBehaviorVisual[]",Array({monster,avatar,unknown,partial}));
    const auto before_loads=loads;const auto defaults=catalog.ReadDefaultVisualConfig();
    Check(roots.empty() && loads==before_loads && defaults.loaded && defaults.behaviors.enumerated && defaults.behaviors.rows.size()==4);
    Check(defaults.behaviors.rows[0].emotion.available && defaults.behaviors.rows[0].emotion.value.empty());
    Check(defaults.behaviors.rows[0].occur.resolved_state.value=="Stagger");
    Check(defaults.behaviors.rows[1].emotion.value=="Emo_Stun" && defaults.behaviors.rows[1].occur.transition.value==0.15f);
    Check(defaults.behaviors.rows[1].occur.resolved_state.value=="Official_Default_Category");
    Check(defaults.behaviors.rows[1].recover.category.value==8 && defaults.behaviors.rows[1].issues.empty());
    Check(defaults.behaviors.rows[1].recover.source_category.value==0 && defaults.behaviors.rows[1].prepared);
    Check(defaults.behaviors.rows[1].loop.category.value==0 && defaults.behaviors.rows[1].loop.resolved_state.value=="Stagger_Loop");
    for(auto source:{monster,avatar,unknown})for(const auto& f:source->fields)
        if(f.name=="AnimRecoverCat")Check(*(reinterpret_cast<std::uint8_t*>(source)+f.offset)==0);
    Check(defaults.behaviors.rows[2].flag.value==147 && defaults.behaviors.rows[3].flag.value==9001 && !defaults.behaviors.rows[3].issues.empty());
    auto character=New();documents[L"visual-character"]=character;
    Field(character,"PhaseAnimConfig","RPG.GameCore.CharacterPhaseAnimConfig",static_cast<void*>(nullptr));
    const auto no_override=catalog.ReadCharacterVisualConfig("visual-character");
    Check(no_override.loaded && !no_override.phase_present && no_override.behaviors.enumerated && no_override.behaviors.rows.empty() && roots.empty());
    auto phase=New();Field(phase,"ModifierBehaviorVisuals","RPG.GameCore.ModifierBehaviorVisual[]",Array({unknown}));
    category_overrides[phase]=L"Character_Phase_Category";
    auto character2=New();documents[L"visual-override"]=character2;Field(character2,"PhaseAnimConfig","RPG.GameCore.CharacterPhaseAnimConfig",phase);
    const auto overridden=catalog.ReadCharacterVisualConfig("visual-override");
    Check(overridden.loaded && overridden.phase_present && overridden.behaviors.rows[0].flag.value==147 && roots.empty());
    Check(overridden.behaviors.rows[0].recover.resolved_state.value=="Character_Phase_Category");
    auto reaction=New();category_overrides[reaction]=L"Effect_Reaction_Category";
    for(auto name:{"Hit","HitH","HitFly","HitFlyRecover","FlyHit","Die","DieHit","DieFly","DieLimbo"})String(reaction,name,L"");
    Field(reaction,"ForceHitH","System.Boolean",static_cast<std::uint8_t>(0));Field(reaction,"OverrideHitAnimWeight","System.Single",0.4f);
    for(const auto& f:unknown->fields)if(f.name=="ReactionAnims")std::memcpy(reinterpret_cast<unsigned char*>(unknown)+f.offset,&reaction,sizeof(reaction));
    const auto reaction_override=catalog.ReadCharacterVisualConfig("visual-override");
    Check(reaction_override.behaviors.rows[0].recover.resolved_state.value=="Effect_Reaction_Category" && reaction_override.behaviors.rows[0].reactions.hit_weight.value==0.4f && roots.empty());
    // Default flag 28 has no phase replacement row, but its category still
    // resolves through the selected character phase. A post-resolution merge
    // of ReadDefaultVisualConfig/ReadCharacterVisualConfig would get this wrong.
    const auto combined=catalog.ReadVisualConfigSources("visual-override");
    Check(combined.context_available && combined.character.phase_present && roots.empty());
    Check(combined.defaults.behaviors.rows[1].flag.value==28 &&
        combined.defaults.behaviors.rows[1].recover.resolved_state.value=="Character_Phase_Category");
    Check(combined.defaults.behaviors.rows[2].recover.resolved_state.value=="Effect_Reaction_Category");
    Check(combined.character.behaviors.rows[0].recover.resolved_state.value=="Effect_Reaction_Category");
    const auto null_context=catalog.ReadVisualConfigSources("visual-character");
    Check(null_context.context_available && !null_context.character.phase_present &&
        null_context.defaults.behaviors.rows[1].recover.resolved_state.value=="Official_Default_Category" && roots.empty());
    const auto missing_context=catalog.ReadVisualConfigSources("missing-visual-character");
    Check(!missing_context.context_available && missing_context.defaults.loaded &&
        !missing_context.character.issues.empty() && roots.empty());
    // A selected modifier phase is the complete replacement phase, not an
    // overlay merged with the base phase. Global rows resolve against it too.
    {
    auto selected_phase=New();selected_phase->type="CharacterPhaseAnimConfig";
    String(selected_phase,"StandBy",L"Phase_Idle");String(selected_phase,"Revive",L"Phase_Revive");
    for(auto name:{"Hit","HitH","HitFly","HitFlyRecover","FlyHit","Die","DieHit","DieFly","DieLimbo"})String(selected_phase,name,L"");
    Field(selected_phase,"ForceHitH","System.Boolean",static_cast<std::uint8_t>(0));
    Field(selected_phase,"OverrideHitAnimWeight","System.Single",-1.0f);
    Field(selected_phase,"ModifierBehaviorVisuals","RPG.GameCore.ModifierBehaviorVisual[]",Array({monster}));
    category_overrides[selected_phase]=L"Modifier_Phase_Category";
    auto change=New();Field(change,"PhaseAnimConfig","RPG.GameCore.CharacterPhaseAnimConfig",selected_phase);
    auto callback=New();Sequence(callback,"Tasks",Array({change}));
    auto modifier=New();Sequence(modifier,"Callbacks",Array({callback}));documents[L"selected-modifier"]=modifier;
    PhaseVisualSource phase_source{PhaseConfigRoot::GlobalModifier,"selected-modifier",{
        {"RPG.GameCore.FixtureConfig","Callbacks",ConfigSelection::Index,0},
        {"RPG.GameCore.FixtureConfig","Tasks",ConfigSelection::Index,0},
        {"RPG.GameCore.FixtureConfig","PhaseAnimConfig"}}};
    auto phase_loads=loads;auto phase_result=catalog.ReadPhaseVisualConfigSources("visual-override",phase_source,input);
    Check(phase_result.issues.empty() && phase_result.visuals.context_available && loads==phase_loads+1 && roots.empty());
    Check(last_definition_root==PhaseConfigRoot::GlobalModifier);
    Check(phase_result.standby.value=="Phase_Idle" && phase_result.revive.value=="Phase_Revive" && phase_result.reactions.hit.available && phase_result.reactions.hit.value.empty());
    Check(phase_result.visuals.character.behaviors.rows.size()==1 && phase_result.visuals.character.behaviors.rows[0].flag.value==6);
    Check(phase_result.visuals.defaults.behaviors.rows[1].occur.resolved_state.value=="Modifier_Phase_Category");
    Check(phase_result.visuals.defaults.behaviors.rows[1].emotion.value=="Emo_Stun");
    Check(phase_result.visuals.defaults.behaviors.rows[2].recover.resolved_state.value=="Effect_Reaction_Category");
    Check(phase_result.source.path.size()==3 && phase_result.visuals.character.source=="selected-modifier.Callbacks[0].Tasks[0].PhaseAnimConfig.ModifierBehaviorVisuals");
    auto bad_source=phase_source;bad_source.path[0].index=7;
    auto invalid_phase=catalog.ReadPhaseVisualConfigSources("visual-override",bad_source,input);
    Check(!invalid_phase.visuals.context_available && !invalid_phase.issues.empty() && roots.empty());
    bad_source=phase_source;bad_source.path[0].declaring_type="RPG.GameCore.AnotherOwner";
    Check(!catalog.ReadPhaseVisualConfigSources("visual-override",bad_source,input).visuals.context_available && roots.empty());
    bad_source=phase_source;bad_source.path.resize(2);
    Check(!catalog.ReadPhaseVisualConfigSources("visual-override",bad_source,input).visuals.context_available && roots.empty());
    const auto original_kind=selected_phase->type;selected_phase->type="AnotherConfig";
    Check(!catalog.ReadPhaseVisualConfigSources("visual-override",phase_source,input).visuals.context_available && roots.empty());
    selected_phase->type=original_kind;
    auto* original_phase=selected_phase;void* null_phase=nullptr;
    std::memcpy(&change->slots[0],&null_phase,sizeof(null_phase));phase_loads=loads;
    auto base_phase=catalog.ReadPhaseVisualConfigSources("visual-override",phase_source,input);
    Check(base_phase.visuals.context_available && loads==phase_loads+2 && roots.empty());
    Check(base_phase.visuals.defaults.behaviors.rows[1].recover.resolved_state.value=="Character_Phase_Category");
    auto no_phase=catalog.ReadPhaseVisualConfigSources("visual-character",phase_source,input);
    Check(no_phase.visuals.context_available && !no_phase.visuals.character.phase_present && no_phase.issues.empty() && roots.empty());
    Check(no_phase.visuals.defaults.behaviors.rows[1].recover.resolved_state.value=="Official_Default_Category");
    std::memcpy(&change->slots[0],&original_phase,sizeof(original_phase));
    // A failed source must not silently turn into the valid base phase.
    bad_source=phase_source;bad_source.name="missing-modifier";
    Check(!catalog.ReadPhaseVisualConfigSources("visual-override",bad_source,input).visuals.context_available && roots.empty());
    auto value_field=New();Field(value_field,"Value","System.Int32",17);documents[L"bad-value"]=value_field;
    bad_source={PhaseConfigRoot::GlobalModifier,"bad-value",{{"RPG.GameCore.FixtureConfig","Value"}}};
    Check(!catalog.ReadPhaseVisualConfigSources("visual-override",bad_source,input).visuals.context_available && roots.empty());
    // List access uses the declared reference-return getter, without copying
    // the whole list. A string-key dictionary follows the same path contract.
    auto list=Array({callback});list->type="Collection";
    auto modifier_list=New();Field(modifier_list,"Callbacks","System.Collections.Generic.List<RPG.GameCore.FixtureConfig>",list);documents[L"modifier-list"]=modifier_list;
    bad_source=phase_source;bad_source.name="modifier-list";
    Check(catalog.ReadPhaseVisualConfigSources("visual-override",bad_source,input).standby.value=="Phase_Idle" && roots.empty());
    // The source row and previously returned value snapshots remain independent.
    Check(phase_result.visuals.defaults.behaviors.rows[1].occur.resolved_state.value=="Modifier_Phase_Category");
    for(const auto& f:monster->fields)if(f.name=="AnimRecoverCat")Check(*(reinterpret_cast<std::uint8_t*>(monster)+f.offset)==0);
    auto phase_dict=Array({nullptr,callback});phase_dict->type="Dictionary";
    auto modifier_dict=New();Field(modifier_dict,"Callbacks","System.Collections.Generic.Dictionary<System.String,RPG.GameCore.FixtureConfig>",phase_dict);documents[L"modifier-dictionary"]=modifier_dict;
    bad_source.name="modifier-dictionary";bad_source.path[0].selection=ConfigSelection::StringKey;bad_source.path[0].key="branch_key";
    Check(catalog.ReadPhaseVisualConfigSources("visual-override",bad_source,input).standby.value=="Phase_Idle" && roots.empty());
    // Ability roots cover both direct tasks and the ability's local modifier
    // library. No global lookup of the local modifier name is involved.
    auto ability_phase=New();Sequence(ability_phase,"OnStart",Array({change}));documents[L"phase-ability"]=ability_phase;
    PhaseVisualSource ability_source{PhaseConfigRoot::Ability,"phase-ability",{
        {"RPG.GameCore.FixtureConfig","OnStart",ConfigSelection::Index,0},
        {"RPG.GameCore.FixtureConfig","PhaseAnimConfig"}}};
    auto direct_phase=catalog.ReadPhaseVisualConfigSources("visual-override",ability_source,input);
    Check(direct_phase.visuals.context_available && direct_phase.standby.value=="Phase_Idle" && roots.empty());
    Check(last_definition_root==PhaseConfigRoot::Ability);
    Check(direct_phase.source.root==PhaseConfigRoot::Ability && direct_phase.source.name=="phase-ability");
    auto local_modifiers=Array({nullptr,modifier});local_modifiers->type="Dictionary";
    Field(ability_phase,"Modifiers","System.Collections.Generic.Dictionary<System.String,RPG.GameCore.FixtureConfig>",local_modifiers);
    ability_source.path=phase_source.path;
    ability_source.path.insert(ability_source.path.begin(),
        {"RPG.GameCore.FixtureConfig","Modifiers",ConfigSelection::StringKey,-1,"branch_key"});
    Check(catalog.ReadPhaseVisualConfigSources("visual-override",ability_source,input).standby.value=="Phase_Idle" && roots.empty());
    auto template_phase=New();Sequence(template_phase,"TaskList",Array({change}));documents[L"phase-template"]=template_phase;
    PhaseVisualSource template_source{PhaseConfigRoot::GlobalTemplate,"phase-template",{
        {"RPG.GameCore.FixtureConfig","TaskList",ConfigSelection::Index,0},
        {"RPG.GameCore.FixtureConfig","PhaseAnimConfig"}}};
    Check(catalog.ReadPhaseVisualConfigSources("visual-override",template_source,input).standby.value=="Phase_Idle" && roots.empty());
    Check(last_definition_root==PhaseConfigRoot::GlobalTemplate);
    template_source.root=static_cast<PhaseConfigRoot>(-1);phase_loads=loads;
    Check(!catalog.ReadPhaseVisualConfigSources("visual-override",template_source,input).visuals.context_available && loads==phase_loads && roots.empty());
    }
    // Warm/stale derived fields must not change output semantics; originals stay
    // untouched even when the official preparer overwrites fields on its copy.
    for(const auto& f:unknown->fields)if(f.name=="AnimRecoverCat")*(reinterpret_cast<std::uint8_t*>(unknown)+f.offset)=99;
    const auto warm=catalog.ReadCharacterVisualConfig("visual-override");
    Check(warm.behaviors.rows[0].recover.source_category.value==99 && warm.behaviors.rows[0].recover.category.value==8 &&
        warm.behaviors.rows[0].recover.resolved_state.value=="Effect_Reaction_Category" && roots.empty());
    for(const auto& f:unknown->fields)if(f.name=="AnimRecoverCat")Check(*(reinterpret_cast<std::uint8_t*>(unknown)+f.offset)==99);
    fail_visual_allocation=true;const auto failed=catalog.ReadDefaultVisualConfig();fail_visual_allocation=false;
    Check(failed.loaded && failed.behaviors.rows.size()==4 && !failed.behaviors.rows[0].prepared &&
        failed.behaviors.rows[0].occur.state.available && !failed.behaviors.rows[0].occur.resolved_state.available && roots.empty());
    const auto count_before_alias=prepared_rows;alias_visual_clone=true;
    const auto alias=catalog.ReadDefaultVisualConfig();alias_visual_clone=false;
    Check(!alias.behaviors.rows[0].prepared && prepared_rows==count_before_alias && roots.empty());
    Field(visual_global,"SkillReadyAnimDefaultTransitDuration","System.Single",0.18f);
    helper_type=New();helper_values={{L"CKPEADONMLA",Text(L"Fixture_DefaultFace")},
        {L"HINPIEHBBMO",Text(L"Fixture_HitFace")},
        {L"MIOIOHJKFEE",Text(L"Fixture_Empty")},{L"PIOGPCGJJAP",Text(L"Fixture_Additive")},
        {L"AIGNNOJHKPJ",Text(L"Fixture_Override")}};
    const auto ready_defaults=catalog.ReadReadyAnimationDefaults();
    Check(ready_defaults.hit_expression.available && ready_defaults.hit_expression.value=="Fixture_HitFace");
    Check(ready_defaults.transition_in.available && ready_defaults.transition_in.value==0.18f && ready_defaults.transitions.size()==6 && roots.empty());
    Check(ready_defaults.transitions.front().from.value==1 && ready_defaults.transitions.front().to.value==2 &&
        ready_defaults.transitions.front().state.value=="Fixture_Fallback");
    Check(ready_defaults.default_expression.available && ready_defaults.default_expression.value=="Fixture_DefaultFace" &&
        ready_defaults.empty_layer_state.value=="Fixture_Empty" && ready_defaults.hit_additive_layer.value=="Fixture_Additive" &&
        ready_defaults.hit_override_layer.value=="Fixture_Override");
    helper_values.erase(L"PIOGPCGJJAP");helper_values[L"CKPEADONMLA"]=nullptr;
    const auto partial_ready=catalog.ReadReadyAnimationDefaults();
    Check(!partial_ready.hit_additive_layer.available && !partial_ready.hit_additive_layer.error.empty() &&
        partial_ready.hit_override_layer.available && partial_ready.default_expression.available &&
    const auto loads_before_hit=loads;
    auto weight_a=New(),weight_b=New();
    Field(weight_a,"m_value","System.Single",0.23f);Field(weight_b,"m_value","System.Single",1.17f);
    Field(visual_global,"ResilienceHitAdditiveWeights","System.Single[]",Array({weight_a,weight_b}));
    Field(visual_global,"AvatarHitAdditiveWeightBase","System.Single",0.91f);
    Field(visual_global,"MonsterHitAdditiveWeightBase","System.Single",0.67f);
    Field(visual_global,"HitFrameMiniInterval","System.Int32",5);
    const auto hit_defaults=catalog.ReadHitAnimationDefaults();
    Check(hit_defaults.resilience_weights.available && hit_defaults.resilience_weights.value==std::vector<float>({0.23f,1.17f}) &&
        hit_defaults.avatar_weight.available && hit_defaults.avatar_weight.value==0.91f && hit_defaults.monster_weight.value==0.67f &&
        hit_defaults.minimum_frame_interval.available && hit_defaults.minimum_frame_interval.value==5 && loads==loads_before_hit && roots.empty());
    const auto hit_names=catalog.ReadDefaultHitNames();
    Check(hit_names.present && hit_names.hit.available && hit_names.hit.value=="Fixture_Hit" &&
        hit_names.hit_h.value=="Fixture_Heavy" && hit_names.hit_fly.value=="Fixture_Launch" && hit_names.fly_hit.value=="Fixture_AirHit" && roots.empty());
    auto hit_character=New();documents[L"receiver"]=hit_character;
    Field(hit_character,"Resilience","RPG.GameCore.ResilienceEnum",std::int32_t(3));
    Field(hit_character,"RepeatOccurAnimWhenBeHitNormalizedTime","System.Single",-1.f);
    Field(hit_character,"PhaseAnimConfig","RPG.GameCore.CharacterPhaseAnimConfig",static_cast<void*>(nullptr));
    const auto hit_loads=loads,hit_prepared=prepared_rows;
    auto receiver=catalog.ReadCharacterHitInput("receiver");
    Check(receiver.loaded && receiver.resilience.available && receiver.resilience.value==3 &&
        receiver.repeat_occur_at.available && receiver.repeat_occur_at.value==-1 && receiver.phase_present.available &&
        !receiver.phase_present.value && !receiver.phase.present && loads==hit_loads+1 && prepared_rows==hit_prepared && roots.empty());
    // Independent scalar failure does not hide a usable phase/name input.
    hit_character->fields.clear();
    auto hit_phase=New();String(hit_phase,"Hit",L"Phase_Hit");
    Field(hit_character,"PhaseAnimConfig","RPG.GameCore.CharacterPhaseAnimConfig",hit_phase);
    receiver=catalog.ReadCharacterHitInput("receiver");
    Check(receiver.loaded && !receiver.resilience.available && receiver.phase_present.available && receiver.phase_present.value &&
        receiver.phase.present && receiver.phase.hit.available && receiver.phase.hit.value=="Phase_Hit" && roots.empty());
    const auto missing_receiver=catalog.ReadCharacterHitInput("missing-receiver");
    Check(!missing_receiver.loaded && !missing_receiver.issues.empty() && roots.empty());
    // The input table's indices are semantic. One unread element cannot shift
    // later weights or invent a default, nor erase independent scalar fields.
    weight_b->fields.clear();const auto partial_hit=catalog.ReadHitAnimationDefaults();
    Check(!partial_hit.resilience_weights.available && partial_hit.resilience_weights.value.empty() &&
        !partial_hit.resilience_weights.error.empty() && partial_hit.avatar_weight.available && roots.empty());
    Field(weight_b,"m_value","System.Single",1.17f);
    const auto saved_global=visual_global;visual_global=nullptr;
    const auto absent_hit=catalog.ReadHitAnimationDefaults();visual_global=saved_global;
    Check(!absent_hit.resilience_weights.available && !absent_hit.avatar_weight.available &&
        !absent_hit.monster_weight.available && !absent_hit.minimum_frame_interval.available && roots.empty());
        partial_ready.default_expression.value.empty() && partial_ready.transitions.size()==6 && roots.empty());
    visual_global=nullptr;const auto unavailable=catalog.ReadDefaultVisualConfig();
    Check(!unavailable.loaded && !unavailable.issues.empty() && roots.empty());
    const auto ready_loads=loads;
    const auto ready=catalog.ReadDefaultReadyTransition(8,11);
    Check(ready.available && ready.value=="Fixture_Ready_Transition" && loads==ready_loads && roots.empty());
    const auto no_ready=catalog.ReadDefaultReadyTransition(-1,0);
    Check(no_ready.available && no_ready.value.empty() && roots.empty());
    ready_query_unavailable=true;const auto missing_ready=catalog.ReadDefaultReadyTransition(8,11);ready_query_unavailable=false;
    Check(!missing_ready.available && !missing_ready.error.empty() && roots.empty());
    // Explicit target-list animation input is another discovery source, not
    // permission to execute its movement ranges or the containing ability.
    auto target_list=New();target_list->type="MoveToTargetList";
    String(target_list,"AnimStateName",L"Not_A_Character_Specific_Name");
    Field(target_list,"AnimLogicState","RPG.GameCore.AnimStateLogicType",3);
    Field(target_list,"TaskEnabled","System.Boolean",static_cast<unsigned char>(1));
    auto caster=New();caster->type="TargetAlias";String(caster,"Alias",L"Caster");
    Field(target_list,"TargetType","RPG.GameCore.TargetEvaluator",caster);
    auto movement_root=New();Sequence(movement_root,"OnStart",Array({target_list}));documents[L"movement-input"]=movement_root;
    const auto movement=catalog.ReadAbilitySources({"movement-input"},input);
    Check(movement.animations.size()==1 && movement.animations[0].kind==AnimationTaskKind::StateWithTargetList &&
        movement.animations[0].state.value=="Not_A_Character_Specific_Name" && movement.animations[0].target_alias.value=="Caster" &&
        movement.animations[0].enabled.value && !movement.animations[0].waits.available && roots.empty());
    // The task query preserves ownership and reference provenance without
    // traversing condition graphs or constructing gameplay components.
    Field(skill,"UseType","RPG.GameCore.SkillUseType",6);
    String(skill,"BoostSkill",L"Boosted");
    Field(skill,"UsableConditions","RPG.GameCore.UsableConditionConfig[]",Array({New(),nullptr}));
    Field(skill,"VisibleCondition","RPG.GameCore.PredicateConfig",static_cast<void*>(nullptr));
    // InsertCondition intentionally unread; it must not become a null source.
    Field(skill,"TargetInfo","RPG.GameCore.SkillTargetConfig",New());
    String(group,"Skill",L"FollowUpCustom");
    auto missing_owner=New();Strings(missing_owner,"AbilityList",Array({Text(L"Unowned")}));
    auto empty_group=New();String(empty_group,"Skill",L"FollowUpCustom");Strings(empty_group,"AbilityList",Array({}));
    for(const auto& f:config->fields)if(f.name=="SkillAbilityList") {
        auto* groups=Array({group,nullptr,empty_group,missing_owner});
        std::memcpy(reinterpret_cast<char*>(config)+f.offset,&groups,sizeof(groups));
    }
    const auto before_task_load=loads;
    const auto tasks=catalog.ReadTaskConfig("battle");
    Check(loads==before_task_load+1 && roots.empty() && tasks.actions.loaded && tasks.skills.size()==1);
    const auto& task_input=tasks.skills.front();
    Check(task_input.skill.source_index==1 && task_input.skill.name.value=="FollowUpCustom" && task_input.use_type.available && task_input.use_type.value==6);
    Check(task_input.boost_skill.value=="Boosted" && task_input.usable_conditions.present.value && task_input.usable_conditions.element_count.value==2);
    Check(task_input.usable_conditions.field=="SkillList[1].UsableConditions" && task_input.usable_conditions.element_count.available);
    Check(task_input.visible_condition.present.available && !task_input.visible_condition.present.value && !task_input.visible_condition.element_count.available);
    Check(!task_input.insert_condition.present.available && !task_input.insert_condition.present.error.empty() && task_input.target.present.value);
    Check(tasks.skill_abilities.enumerated && tasks.skill_abilities.declared_count==4 && tasks.skill_abilities.rows.size()==3);
    Check(tasks.skill_abilities.rows[0].skill.value=="FollowUpCustom" && tasks.skill_abilities.rows[0].abilities.values.size()==2);
    Check(tasks.skill_abilities.rows[1].source_index==2 && tasks.skill_abilities.rows[1].abilities.available && tasks.skill_abilities.rows[1].abilities.values.empty());
    Check(!tasks.skill_abilities.rows[2].skill.available && tasks.skill_abilities.rows[2].abilities.values[0]=="Unowned");
    Check(tasks.issues.size()==2 && tasks.actions.abilities.size()==5); // missing owner does not discard abilities
    for(const auto& f:skill->fields)if(f.name=="UsableConditions") {
        auto* empty=Array({});std::memcpy(reinterpret_cast<char*>(skill)+f.offset,&empty,sizeof(empty));
    }
    const auto empty_conditions=catalog.ReadTaskConfig("battle");
    Check(empty_conditions.skills[0].usable_conditions.present.value && empty_conditions.skills[0].usable_conditions.element_count.value==0);
    nodes.clear(); documents.clear();
    reflected_type=reflected_field=nullptr;
    helper_type=nullptr;helper_values.clear();
    category_overrides.clear();
    clone_sources.clear();
}
