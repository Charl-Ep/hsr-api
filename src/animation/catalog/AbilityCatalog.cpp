#include "CatalogReader.hpp"

namespace hksr::sdk_v1 {
struct OfficialCatalog::State::AbilityRead final : AbilitySourceRead {
    std::shared_ptr<State> owner;
    State& s; TypeRelationships& types; State::Scope scope;
    AbilitySources output;
    struct Edge { ConfigStep step; void* object; };
    struct Node {
        std::optional<AnimationTaskSource> animation;
        std::optional<AnimationWaitSource> wait;
        std::optional<EffectRemovalSource> removal;
        std::optional<AbilityReferenceSource> reference;
        std::optional<SkillMappingSource> skill_mapping;
        std::optional<SkillContinuationSource> skill_continuation;
        std::vector<Edge> edges; std::vector<std::string> issues;
    };
    std::map<void*,Node> cache;
    std::map<void*,TextInput> target_aliases;
    std::map<std::string,bool> config_types;
    std::vector<std::string> abilities;
    std::size_t next_ability=0;
    std::string ability;
    std::set<std::string> visited_abilities;
    std::set<void*> visited;
    struct Work { void* object; std::vector<ConfigStep> path; };
    std::vector<Work> pending;
    Method load{};
    bool initialized=false,draining=false,done=false,cancelled=false;
    std::optional<Work> current;
    Node building;
    std::vector<DeclaredField> fields;
    std::size_t next_field=0;
    struct SequenceCursor {
        void* array=nullptr;void* dictionary=nullptr;Method lookup{};
        int index=0,count=0;
        std::string owner,field;
    } sequence;
    AbilityRead(std::shared_ptr<State> state,std::vector<std::string> names,TypeRelationships& relation)
        :owner(std::move(state)),s(*owner),types(relation),scope(s),abilities(std::move(names)){}
    bool config_type(const std::string& qualified) {
        auto found=config_types.find(qualified); if(found!=config_types.end()) return found->second;
        Require(qualified.find_first_of("<[]>&*")==std::string::npos,"unsupported constructed task declaration");
        const auto split=qualified.find_last_of('.');
        const auto space=split==std::string::npos?std::string():qualified.substr(0,split);
        const auto name=split==std::string::npos?qualified:qualified.substr(split+1);
        const bool result=types.TypeDerivesFrom(space.c_str(),name.c_str(),"RPG.GameCore","JsonConfig");
        config_types.emplace(qualified,result); return result;
    }
    bool actual_config(void* object) {
        const auto type=s.metadata.ReadObjectTypeName(object);
        return config_type(type.name_space+"."+type.name);
    }
    void BeginNode(void* object) {
        auto& node=building;
        try {
            scope.Keep({{true,0},object}); const auto type=s.metadata.ReadObjectTypeName(object);
            const auto name=type.name_space+"."+type.name;
            Require(config_type(name),"task graph node is not JsonConfig-derived");
            fields=s.metadata.ReadObjectFields(object);
            auto capture=[&](auto& value,const char* field,auto read) {
                try { value.value=read(); value.available=true; }
                catch(const std::exception& error) { value.error=error.what(); node.issues.push_back(std::string(field)+": "+error.what()); }
            };
            auto target_alias=[&]() {
                const auto target=Read<void*>(object,Field(fields,"TargetType","RPG.GameCore.TargetEvaluator"));
                Require(target!=nullptr,"target is null, not a literal Caster");
                auto found=target_aliases.find(target);
                if(found==target_aliases.end()) {
                    TextInput value;
                    try {
                        scope.Keep({{true,0},target});
                        const auto target_type=s.metadata.ReadObjectTypeName(target);
                        Require(target_type.name_space=="RPG.GameCore" && target_type.name=="TargetAlias","target is not a literal alias");
                        value={true,s.String(target,s.metadata.ReadObjectFields(target),"Alias"),{}};
                    }catch(const std::exception& e){value.error=e.what();}
                    found=target_aliases.emplace(target,std::move(value)).first;
                }
                if(!found->second.available)throw std::runtime_error(found->second.error);
                return found->second.value;
            };
            if(name=="RPG.GameCore.TriggerAnimState" || name=="RPG.GameCore.TriggerAnimStateWithMove" || name=="RPG.GameCore.MoveToTargetList") {
                AnimationTaskSource entry;
                entry.kind=name=="RPG.GameCore.TriggerAnimState"?AnimationTaskKind::State:
                    name=="RPG.GameCore.TriggerAnimStateWithMove"?AnimationTaskKind::StateWithMove:AnimationTaskKind::StateWithTargetList;
                // Discover the literal animation input, not an executable
                // movement/target graph. MoveToTargetPosition has no such input.
                capture(entry.state,"AnimStateName",[&] { return s.String(object,fields,"AnimStateName"); });
                capture(entry.logic_kind,"AnimLogicState",[&] { return Read<int>(object,Field(fields,"AnimLogicState","RPG.GameCore.AnimStateLogicType")); });
                capture(entry.enabled,"TaskEnabled",[&] { return Read<unsigned char>(object,Field(fields,"TaskEnabled","System.Boolean"))!=0; });
                if(entry.kind==AnimationTaskKind::StateWithMove)
                    capture(entry.waits,"WaitAnimState",[&] { return Read<unsigned char>(object,Field(fields,"WaitAnimState","System.Boolean"))!=0; });
                // Optional relationship information must not hide an otherwise
                // discoverable animation with a computed or missing target.
                try {entry.target_alias={true,target_alias(),{}};}
                catch(const std::exception& e){entry.target_alias.error=e.what();}
                node.animation=std::move(entry);
            }
            if(name=="RPG.GameCore.WaitAnimState") {
                AnimationWaitSource entry;
                capture(entry.state,"AnimStateName",[&]{return s.String(object,fields,"AnimStateName");});
                capture(entry.target_alias,"TargetType",target_alias);
                capture(entry.enabled,"TaskEnabled",[&]{return Read<unsigned char>(object,Field(fields,"TaskEnabled","System.Boolean"))!=0;});
                node.wait=std::move(entry);
            }
            if(name=="RPG.GameCore.RemoveEffect") {
                EffectRemovalSource entry;auto& value=entry.input;
                capture(value.effect_path,"EffectPath",[&]{return s.String(object,fields,"EffectPath");});
                capture(value.path_key,"EffectPathKey",[&]{return s.String(object,fields,"EffectPathKey");});
                capture(value.target_alias,"TargetType",target_alias);
                capture(value.enabled,"TaskEnabled",[&]{return Read<unsigned char>(object,Field(fields,"TaskEnabled","System.Boolean"))!=0;});
                node.removal=std::move(entry);
            }
            if(name=="RPG.GameCore.TriggerAbility") {
                AbilityReferenceSource entry;
                capture(entry.enabled,"TaskEnabled",[&]{return Read<unsigned char>(object,Field(fields,"TaskEnabled","System.Boolean"))!=0;});
                capture(entry.target,"AbilityName",[&] {
                    // DynamicString is a reference type (4.6 runtime type
                    // query). Only a literal value is a discovery edge; never
                    // evaluate its key or execute the containing ability.
                    auto value=Read<void*>(object,Field(fields,"AbilityName","RPG.GameCore.DynamicString"));
                    Require(value!=nullptr,"ability name absent");
                    const auto declared=s.metadata.ReadObjectFields(value);
                    Require(!Read<unsigned char>(value,Field(declared,"Custom","System.Boolean")),"dynamic ability name requires runtime context");
                    return s.String(value,declared,"Value");
                });
                node.reference=std::move(entry);
            }
            if(name=="RPG.GameCore.SetControlSkillMapping") {
                SkillMappingSource entry;
                capture(entry.skill,"SkillTriggerKey",[&]{return s.String(object,fields,"SkillTriggerKey");});
                capture(entry.target_alias,"TargetType",target_alias);
                capture(entry.control,"ControlSkillType",[&]{return Read<int>(object,Field(fields,"ControlSkillType","RPG.GameCore.ControlSkillType"));});
                capture(entry.enabled,"TaskEnabled",[&]{return Read<unsigned char>(object,Field(fields,"TaskEnabled","System.Boolean"))!=0;});
                node.skill_mapping=std::move(entry);
            }
            if(name=="RPG.GameCore.UseSkillOneMore") {
                SkillContinuationSource entry;
                capture(entry.control,"SkillType",[&]{return Read<int>(object,Field(fields,"SkillType","RPG.GameCore.ControlSkillType"));});
                capture(entry.child,"ChildSkillIndex",[&]{return Read<int>(object,Field(fields,"ChildSkillIndex","System.Int32"));});
                capture(entry.enabled,"TaskEnabled",[&]{return Read<unsigned char>(object,Field(fields,"TaskEnabled","System.Boolean"))!=0;});
                node.skill_continuation=std::move(entry);
            }
        } catch(const std::exception& error) { node.issues.push_back(error.what()); }
    }
    void Add(ConfigStep step,void* child) {
        if(child && actual_config(child)) {
            // Cached edges survive across frames even before the child is
            // visited. Retain that actual object, not just the source container.
            scope.Keep({{true,0},child});
            building.edges.push_back({std::move(step),child});
        }
    }
    void BeginField(const DeclaredField& field) {
        const auto owner_name=field.owner_namespace+"."+field.owner_name;
        try {
            Require(field.flags_observed,"field static/instance flags unavailable");
            if(field.flags&0x10 || field.type=="System.String") return;
            const auto& ft=field.type;
            const bool array=ft.size()>2 && ft.substr(ft.size()-2)=="[]";
            const bool list=ft.compare(0,32,"System.Collections.Generic.List<")==0;
            const std::string dictionary="System.Collections.Generic.Dictionary<System.String,";
            SequenceCursor next;next.owner=owner_name;next.field=field.name;
            if(array || list) {
                const auto element=array?ft.substr(0,ft.size()-2):ft.substr(32,ft.size()-33);
                if(element!="System.Object" && !config_type(element)) return;
                next.array=s.Sequence(current->object,fields,field.name.c_str(),scope);
                if(!next.array)return;
                scope.Keep({{true,0},next.array});
                const auto count=s.arrays.Length(next.array);
                Require(count.read.returned && count.value>=0,"sequence length unavailable");next.count=count.value;
                sequence=std::move(next);
            } else if(ft.compare(0,dictionary.size(),dictionary)==0 && ft.back()=='>') {
                const auto value_type=ft.substr(dictionary.size(),ft.size()-dictionary.size()-1);
                if(!config_type(value_type)) return; // prove reference-return ABI before get_Item
                next.dictionary=Read<void*>(current->object,field);if(!next.dictionary)return;
                scope.Keep({{true,0},next.dictionary});
                const auto declaration=s.metadata.ReadObjectType(next.dictionary);
                auto keys=scope.Keep(s.Call(Getter(declaration,"get_Keys"),next.dictionary));
                const auto keys_type=s.metadata.ReadObjectType(keys);
                Require(native::Call(Instance(keys_type,"get_Count","System.Int32"),&next.count,keys).returned && next.count>=0,"dictionary key count unavailable");
                auto object_type=scope.Keep(s.objects.ManagedType("System","Object"));
                next.array=scope.Keep(s.arrays.Create(object_type,next.count));
                Require(native::CallVoid(Instance(keys_type,"System.Collections.ICollection.CopyTo","System.Void",{"System.Array","System.Int32"}),keys,next.array,0).returned,"dictionary key copy failed");
                next.lookup=Instance(declaration,"get_Item",value_type.c_str(),{"System.String"});
                sequence=std::move(next);
            } else if(ft.compare(0,27,"System.Collections.Generic.")==0) {
                building.issues.push_back(owner_name+"."+field.name+": unsupported container "+ft);
            } else if(ft.compare(0,7,"System.")!=0 && config_type(ft)) {
                Add({owner_name,field.name},Read<void*>(current->object,field));
            }
        }catch(const std::exception& e){building.issues.push_back(owner_name+"."+field.name+": "+e.what());}
    }
    void ReadElement() {
        const auto i=sequence.index++;
        try {
            const auto item=s.arrays.Get(sequence.array,i);Require(item.call.returned,"sequence item unavailable");
            if(sequence.dictionary) {
                Require(item.object!=nullptr,"dictionary key unavailable");
                const auto child=s.Call(sequence.lookup,sequence.dictionary,item.object);
                Require(child.call.returned,"dictionary value lookup failed");
                Add({sequence.owner,sequence.field,ConfigSelection::StringKey,-1,s.Text(item.object)},child.object);
            }else Add({sequence.owner,sequence.field,ConfigSelection::Index,i,{}},item.object);
        }catch(const std::exception& e){building.issues.push_back(sequence.owner+"."+sequence.field+"["+std::to_string(i)+"]: "+e.what());}
    }
    void Emit(const Work& work,const Node& node) {
        if(node.animation) {auto entry=*node.animation;entry.ability=ability;entry.path=work.path;output.animations.push_back(std::move(entry));}
        if(node.wait) {auto entry=*node.wait;entry.ability=ability;entry.path=work.path;output.waits.push_back(std::move(entry));}
        if(node.removal) {auto entry=*node.removal;entry.ability=ability;entry.path=work.path;output.removals.push_back(std::move(entry));}
        if(node.reference) {auto entry=*node.reference;entry.ability=ability;entry.path=work.path;output.references.push_back(std::move(entry));}
        if(node.skill_mapping) {auto entry=*node.skill_mapping;entry.ability=ability;entry.path=work.path;output.skill_mappings.push_back(std::move(entry));}
        if(node.skill_continuation) {auto entry=*node.skill_continuation;entry.ability=ability;entry.path=work.path;output.skill_continuations.push_back(std::move(entry));}
        // Preserve repeated ordered slots while expanding a shared/cyclic node
        // once per root. Cache lifetime covers ALL roots, not just one frame.
        if(!visited.insert(work.object).second)return;
        for(const auto& issue:node.issues)output.issues.push_back({ability,issue,work.path});
        for(auto edge=node.edges.rbegin();edge!=node.edges.rend();++edge) {
            auto path=work.path;path.push_back(edge->step);pending.push_back({edge->object,std::move(path)});
        }
    }
    void Step() {
        if(draining) {
            if(!scope.ids.empty())scope.ReleaseOne();
            else {done=true;if(cancelled)output={};}
            return;
        }
        if(!initialized) {
            initialized=true;
            try {if(!abilities.empty())load=s.Static("AbilityJsonTable","GetTurnBasedAbility","RPG.GameCore.TurnBasedAbilityConfig",{"System.String"});}
            catch(const std::exception& e){output.issues.push_back({{},e.what(),{}});draining=true;}
            return;
        }
        if(current) {
            if(sequence.index<sequence.count){ReadElement();return;}
            if(next_field<fields.size()){BeginField(fields[next_field++]);return;}
            auto found=cache.emplace(current->object,std::move(building)).first;++output.nodes_read;
            Emit(*current,found->second);current.reset();fields.clear();return;
        }
        if(!pending.empty()) {
            auto work=std::move(pending.back());pending.pop_back();
            const auto found=cache.find(work.object);
            if(found!=cache.end())Emit(work,found->second);
            else {current=std::move(work);building={};fields.clear();next_field=0;sequence={};BeginNode(current->object);}
            return;
        }
        if(next_ability==abilities.size()){draining=true;return;}
        ability=abilities[next_ability++];
        if(!visited_abilities.insert(ability).second)return;
        visited.clear();
        try {
            Require(!ability.empty(),"ability key empty");
            auto input=scope.Keep(s.objects.NewStringUtf8(ability.c_str()));
            auto root=scope.Keep(s.Call(load,input));pending.push_back({root,{}});
        }catch(const std::exception& e){output.issues.push_back({ability,e.what(),{}});}
    }
    void Advance(std::uint32_t budget_microseconds) override {
        const auto start=std::chrono::steady_clock::now();
        // Even a zero budget makes one unit of progress (also deterministic for
        // tests). Expiry pauses; it never rejects or truncates the graph.
        if(done)return;
        do {Step();}while(!done && std::chrono::steady_clock::now()-start<std::chrono::microseconds(budget_microseconds));
    }
    void Cancel() override {cancelled=true;draining=true;if(done)output={};}
    bool Done() const override {return done;}
    AbilitySources Take() override {Require(done,"ability discovery still in progress");return std::move(output);}
};
std::unique_ptr<AbilitySourceRead> OfficialCatalog::BeginAbilitySources(std::vector<std::string> abilities,TypeRelationships& types) {
    return std::make_unique<State::AbilityRead>(*owner_,std::move(abilities),types);
}
AbilitySources OfficialCatalog::ReadAbilitySources(const std::vector<std::string>& abilities,TypeRelationships& types) {
    auto read=BeginAbilitySources(abilities,types);
    while(!read->Done())read->Advance(2000);
    return read->Take();
}
}
