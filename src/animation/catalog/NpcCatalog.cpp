#include "CatalogReader.hpp"

namespace hksr::sdk_v1 {
NpcMotionRow OfficialCatalog::State::MotionRow(Method lookup,std::uint32_t id,std::string indexed_name) {
        Scope scope(*this); auto row=scope.Keep(Call(lookup,id)); const auto fields=metadata.ReadObjectFields(row);
        Require(Read<std::uint32_t>(row,Field(fields,"ID","System.UInt32"))==id,"motion row returned another ID");
        NpcMotionRow value; value.id=id; value.indexed_name=std::move(indexed_name);
        auto field=[&](const char* key) { TextInput part;
            try { part.value=String(row,fields,key); part.available=true; }
            catch(const std::exception& error) { part.error=error.what(); } return part; };
        value.name=field("StartMotion"); value.source_character=field("FreeStyleCharacterID");
        value.start_path=field("StartMotionPath"); value.loop_path=field("LoopMotionPath"); return value;
    }
NpcMotionIndex OfficialCatalog::ReadNpcMotionIndex(const std::string& character) {
    NpcMotionIndex output; output.character=character; auto& batch=output.motions; State::Scope scope(*state_);
    try {
        Require(!character.empty(),"FreeStyle character identity absent");
        auto input=scope.Keep(state_->objects.NewStringUtf8(character.c_str()));
        const auto names=state_->methods.Resolve("",detail::catalog_bindings::npc_motions,detail::catalog_bindings::motion_names,"System.Collections.Generic.List<System.String>",{"System.String"},MethodKind::Static);
        const auto id_for_name=state_->methods.Resolve("",detail::catalog_bindings::npc_motions,detail::catalog_bindings::motion_id,"System.UInt32",{"System.String","System.String"},MethodKind::Static);
        const auto lookup=state_->methods.Resolve("",detail::catalog_bindings::npc_motions,detail::catalog_bindings::motion_row,"RPG.GameCore.FreeStyleMotionConfigRow",{"System.UInt32"},MethodKind::Static);
        auto result=state_->Call(names,input); Require(result.call.returned,"motion index lookup failed");
        if(!result.object) { batch.enumerated=true; return output; }
        auto list=scope.Keep(result);
        auto access=ManagedReferenceList(ResolveReferenceListMethods(state_->metadata.ReadObjectType(list),"System.String"));
        auto count=access.Count(list); Require(count.read.returned && count.value>=0,"motion index count unavailable");
        batch.declared_count=count.value;
        for(int i=0;i<count.value;++i) {
            ++batch.visited_count;
            try {
                auto name=access.Item(list,i); Require(name.call.returned && name.object,"motion index name missing");
                NpcMotionRow value; value.indexed_name=state_->Text(name.object);
                Require(native::Call(id_for_name,&value.id,input,name.object).returned && value.id,"motion ID lookup failed");
                batch.rows.push_back(state_->MotionRow(lookup,value.id,std::move(value.indexed_name)));
            } catch(const std::exception& error) { batch.issues.push_back({i,"motion row",error.what()}); }
        }
        batch.enumerated=true;
    } catch(const std::exception& error) { batch.issues.push_back({-1,"motion index",error.what()}); }
    return output;
}
NpcMotionSources OfficialCatalog::ReadNpcMotions(const std::string& character,const std::string& character_config) {
    NpcMotionSources output; output.character_config=character_config;
    output.index=ReadNpcMotionIndex(character); std::set<std::uint32_t> ids;
    for(const auto& row:output.index.motions.rows) if(ids.insert(row.id).second) output.motions.push_back(row);
    std::set<std::uint32_t> configured;
    State::Scope scope(*state_);
    try {
        auto config=state_->Config("FreeStyleCharacterConfig",character_config,scope);
        output.state_config=state_->String(config,state_->metadata.ReadObjectFields(config),"FreeStyleStateConfigPath");
        auto states=state_->Config("FreeStyleStateConfig",output.state_config,scope);
        const auto fields=state_->metadata.ReadObjectFields(states);
        auto array=state_->Sequence(states,fields,"StateList",scope);
        if(array) {
            auto length=state_->arrays.Length(array); Require(length.read.returned && length.value>=0,"StateList length unavailable");
            for(int i=0;i<length.value;++i) {
                State::Scope row_scope(*state_);
                try {
                    auto item=state_->arrays.Get(array,i); Require(item.call.returned,"StateList item unreadable"); if(!item.object) continue;
                    auto row=row_scope.Keep(item); const auto row_fields=state_->metadata.ReadObjectFields(row);
                    for(const auto* name:{"MotionID","SameAsMotionIDOnLoop"}) try {
                        auto id=Read<std::uint32_t>(row,Field(row_fields,name,"System.UInt32")); if(id) configured.insert(id);
                    } catch(const std::exception& error) { output.issues.push_back({i,name,error.what()}); }
                    try {
                        auto entrances=state_->Sequence(row,row_fields,"EntranceFreeStyleIDList",row_scope,"System.UInt32");
                        if(entrances) {
                            auto size=state_->arrays.Length(entrances); Require(size.read.returned && size.value>=0,"entrance array length unavailable");
                            Method convert{};
                            if(size.value) convert=state_->methods.Resolve("System","Convert","ToUInt32","System.UInt32",{"System.Object"},MethodKind::Static);
                            for(int j=0;j<size.value;++j) try {
                                State::Scope number_scope(*state_); auto number=number_scope.Keep(state_->arrays.Get(entrances,j)); std::uint32_t id=0;
                                Require(native::Call(convert,&id,number).returned,"boxed entrance ID conversion failed"); if(id) configured.insert(id);
                            } catch(const std::exception& error) { output.issues.push_back({i,"EntranceFreeStyleIDList["+std::to_string(j)+"]",error.what()}); }
                        }
                    } catch(const std::exception& error) { output.issues.push_back({i,"EntranceFreeStyleIDList",error.what()}); }
                } catch(const std::exception& error) { output.issues.push_back({i,"StateList row",error.what()}); }
            }
        }
        output.state_list_read=true;
    } catch(const std::exception& error) { output.issues.push_back({-1,"FreeStyle state source",error.what()}); }
    output.configured_ids.assign(configured.begin(),configured.end());
    Method lookup{};
    try { if(!configured.empty()) lookup=state_->methods.Resolve("",detail::catalog_bindings::npc_motions,detail::catalog_bindings::motion_row,"RPG.GameCore.FreeStyleMotionConfigRow",{"System.UInt32"},MethodKind::Static); }
    catch(const std::exception& error) { output.issues.push_back({-1,"configured motion lookup",error.what()}); return output; }
    for(auto id:configured) if(!ids.count(id)) try {
        output.motions.push_back(state_->MotionRow(lookup,id)); ids.insert(id);
    } catch(const std::exception& error) { output.issues.push_back({-1,"configured motion "+std::to_string(id),error.what()}); }
    return output;
}
NpcMotionSources OfficialCatalog::ReadNpcMotionsFromComponent(void* component) {
    State::Scope scope(*state_); std::string character,path; std::vector<CatalogIssue> issues;
    try {
        scope.Keep({{true,0},component});
        auto read=[&](const char* getter,std::string& destination) {
            try {
                auto method=state_->methods.Resolve("RPG.GameCore","FreeStyleComponent",getter,"System.String",{});
                auto result=state_->Call(method,component); Require(result.call.returned,"FreeStyle identity getter failed");
                if(result.object) { scope.Keep(result); destination=state_->Text(result.object); }
            } catch(const std::exception& error) { issues.push_back({-1,getter,error.what()}); }
        };
        read("get_FreeStyleCharacterID",character); read("get_FreeStyleCharacterConfigPath",path);
    } catch(const std::exception& error) { issues.push_back({-1,"FreeStyle component",error.what()}); }
    auto output=ReadNpcMotions(character,path);
    output.issues.insert(output.issues.end(),issues.begin(),issues.end()); return output;
}
}
