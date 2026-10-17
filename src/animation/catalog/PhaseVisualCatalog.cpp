#include "CatalogReader.hpp"

namespace hksr::sdk_v1 {
PhaseVisualConfigSources OfficialCatalog::ReadPhaseVisualConfigSources(const std::string& character,
    const PhaseVisualSource& source,TypeRelationships& types) {
    PhaseVisualConfigSources output;output.source=source;
    auto& s=*state_;State::Scope scope(s);void* phase=nullptr;
    std::string location=source.name,error;
    auto keep=[&](void* object){return object?scope.Keep(ObjectResult{{true,0},object}):nullptr;};
    auto reference=[&](const std::string& name) {
        const auto dot=name.rfind('.');
        Require(dot!=std::string::npos && name.find_first_of("<[]>&*")==std::string::npos,
            "phase path requires a concrete managed reference");
        Require(!types.TypeDerivesFrom(name.substr(0,dot).c_str(),name.substr(dot+1).c_str(),"System","ValueType"),
            "phase path cannot reinterpret a value-type field");
    };
    try {
        Require(!source.name.empty() && source.name.find('\0')==std::string::npos && !source.path.empty(),
            "phase definition source is empty or invalid");
        const char* getter=nullptr;const char* root_type=nullptr;
        switch(source.root) {
        case PhaseConfigRoot::Ability:
            getter="GetTurnBasedAbility";root_type="RPG.GameCore.TurnBasedAbilityConfig";break;
        case PhaseConfigRoot::GlobalModifier:
            getter="GetGlobalModifierConfig";root_type="RPG.GameCore.TurnBasedModifierConfig";break;
        case PhaseConfigRoot::GlobalTemplate:
            getter="GetGlobalTaskListTemplate";root_type="RPG.GameCore.TaskListTemplate";break;
        default:throw std::invalid_argument("unknown phase definition root");
        }
        auto* name=scope.Keep(s.objects.NewStringUtf8(source.name.c_str()));
        phase=scope.Keep(s.Call(s.Static("AbilityJsonTable",getter,root_type,{"System.String"}),name));
        std::string selected_type;
        for(const auto& step:source.path) {
            Require(phase!=nullptr,"phase path has a null intermediate node");
            const auto fields=s.metadata.ReadObjectFields(phase);DeclaredField selected;unsigned matches=0;
            for(const auto& f:fields)if(f.owner_namespace+"."+f.owner_name==step.declaring_type && f.name==step.field &&
                f.flags_observed && !(f.flags&0x10)){selected=f;++matches;}
            Require(matches==1,"phase path field is absent or ambiguous");
            location+="."+step.field;
            if(step.selection==ConfigSelection::Reference) {
                selected_type=selected.type;reference(selected_type);phase=keep(Read<void*>(phase,selected));
            } else if(step.selection==ConfigSelection::Index) {
                const auto& type=selected.type;const std::string prefix="System.Collections.Generic.List<";
                const bool array=type.size()>2 && type.substr(type.size()-2)=="[]";
                const bool list=type.compare(0,prefix.size(),prefix)==0 && type.back()=='>';
                Require(array || list,"phase index source is not an array or List");
                const auto element=array?type.substr(0,type.size()-2):type.substr(prefix.size(),type.size()-prefix.size()-1);
                selected_type=element;reference(element);
                auto* sequence=keep(Read<void*>(phase,selected));
                Require(sequence && step.index>=0,"phase sequence/index is absent");
                location+="["+std::to_string(step.index)+"]";
                if(array) {
                    auto count=s.arrays.Length(sequence);Require(count.read.returned && step.index<count.value,"phase array index outside source");
                    auto item=s.arrays.Get(sequence,step.index);Require(item.call.returned,"phase array item unread");phase=keep(item.object);
                } else {
                    const auto declared=s.metadata.ReadObjectType(sequence);
                    auto value=s.Call(Instance(declared,"get_Item",element.c_str(),{"System.Int32"}),sequence,step.index);
                    Require(value.call.returned,"phase list item unread");phase=keep(value.object);
                }
            } else if(step.selection==ConfigSelection::StringKey) {
                // Dictionary signatures supply the reference-return ABI; do not
                // infer it from the runtime object address or from a value slot.
                Require(selected.type.find("System.Collections.Generic.Dictionary<System.String,")==0,
                    "phase key source is not a string dictionary");
                auto* dictionary=keep(Read<void*>(phase,selected));Require(dictionary!=nullptr,"phase dictionary is null");
                const auto declared=s.metadata.ReadObjectType(dictionary);Method item{};std::string result;unsigned found=0;
                for(const auto& m:declared.methods)if(m.name=="get_Item" && !m.is_static && m.parameters==std::vector<std::string>{"System.String"}){
                    item=m.entry;result=m.result;++found;
                }
                Require(found==1,"phase dictionary getter is absent or ambiguous");selected_type=result;reference(result);
                auto* key=scope.Keep(s.objects.NewStringUtf8(step.key.c_str()));
                auto value=s.Call(item,dictionary,key);Require(value.call.returned,"phase dictionary item unread");phase=keep(value.object);
                location+="["+step.key+"]";
            } else throw std::runtime_error("unknown phase path selection");
        }
        Require(selected_type=="RPG.GameCore.CharacterPhaseAnimConfig","phase path does not select a declared CharacterPhaseAnimConfig");
        if(phase) {
            const auto actual=s.metadata.ReadObjectTypeName(phase);
            Require(actual.name_space=="RPG.GameCore" && actual.name=="CharacterPhaseAnimConfig",
                "selected data is not CharacterPhaseAnimConfig");
        } else {
            auto* base=s.Config("CharacterConfig",character,scope);
            phase=keep(Read<void*>(base,Field(s.metadata.ReadObjectFields(base),"PhaseAnimConfig","RPG.GameCore.CharacterPhaseAnimConfig")));
            location=character+".PhaseAnimConfig";
        }
    }catch(const std::exception& e){error=e.what();phase=nullptr;output.issues.push_back({-1,location,error});}
    output.visuals=s.VisualSources(character,phase,error.empty(),location);
    if(!error.empty())output.visuals.character.issues.push_back({-1,location,error});
    if(error.empty() && phase)try {
        const auto fields=s.metadata.ReadObjectFields(phase);
        auto capture=[&](auto& out,const char* name,auto read) {
            try {out.value=read();out.available=true;}
            catch(const std::exception& e){out.error=e.what();output.issues.push_back({-1,name,e.what()});}
        };
        auto text=[&](TextInput& out,const char* name){capture(out,name,[&]{return s.String(phase,fields,name);});};
        text(output.standby,"StandBy");text(output.revive,"Revive");output.reactions.present=true;
        auto& r=output.reactions;text(r.hit,"Hit");text(r.hit_h,"HitH");text(r.hit_fly,"HitFly");text(r.hit_fly_recover,"HitFlyRecover");
        text(r.fly_hit,"FlyHit");text(r.die,"Die");text(r.die_hit,"DieHit");text(r.die_fly,"DieFly");text(r.die_limbo,"DieLimbo");
        capture(r.force_hit_h,"ForceHitH",[&]{return Read<std::uint8_t>(phase,Field(fields,"ForceHitH","System.Boolean"))!=0;});
        capture(r.hit_weight,"OverrideHitAnimWeight",[&]{return Read<float>(phase,Field(fields,"OverrideHitAnimWeight","System.Single"));});
    }catch(const std::exception& e){output.issues.push_back({-1,"phase fields",e.what()});}
    return output;
}
}
