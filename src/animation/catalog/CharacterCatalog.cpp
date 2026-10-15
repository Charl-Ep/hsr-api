#include "CatalogReader.hpp"

namespace hksr::sdk_v1 {
CatalogBatch<PlayerCatalogRow> OfficialCatalog::State::Players() {
        CatalogBatch<PlayerCatalogRow> output; Scope scope(*this);
        try {
            auto list=scope.Keep(Call(Static("AdventurePlayerCommonWrap","GetAllWrapData",
                "System.Collections.Generic.List<RPG.GameCore.IAdventurePlayerCommonRowWrap>",{"System.Boolean"}),false));
            auto access=ManagedReferenceList(ResolveReferenceListMethods(metadata.ReadObjectType(list),"RPG.GameCore.IAdventurePlayerCommonRowWrap"));
            auto count=access.Count(list); Require(count.read.returned && count.value>=0,"player wrapper count unavailable");
            output.declared_count=count.value;
            for(int i=0;i<count.value;++i) {
                ++output.visited_count;
                try {
                    auto item=access.Item(list,i); Require(item.call.returned && item.object,"player wrapper missing");
                    const auto type=metadata.ReadObjectType(item.object); PlayerCatalogRow row;
                    auto number=[&](const char* name,std::uint32_t& value) {
                        Require(native::Call(Instance(type,name,"System.UInt32"),&value,item.object).returned,"wrapper ID getter failed");
                    };
                    number("get_ID",row.id); number("get_AvatarID",row.avatar);
                    number("get_EnhancedID",row.enhanced); number("get_DevSkinID",row.skin);
                    auto string=[&](const char* name) { auto value=Call(Instance(type,name,"System.String"),item.object);
                        Require(value.call.returned,"wrapper path getter failed");
                        Scope string_scope(*this); if(value.object) string_scope.Keep(value); return Text(value.object); };
                    row.character_config=string("get_PlayerJsonPath"); row.prefab=string("get_PlayerPrefabPath");
                    output.rows.push_back(std::move(row));
                } catch(const std::exception& error) { output.issues.push_back({i,"player wrapper",error.what()}); }
            }
            output.enumerated=true;
        } catch(const std::exception& error) { output.issues.push_back({-1,"GetAllWrapData",error.what()}); }
        return output;
    }
CharacterCatalog OfficialCatalog::ReadCharacters() {
    CharacterCatalog output;
    state_->Table(output.npcs,"NPCDataExcelTable","RPG.GameCore.NPCDataRow",[&](void* row,const auto& fields) {
        return NpcCatalogRow{Read<std::uint32_t>(row,Field(fields,"ID","System.UInt32")),
            state_->String(row,fields,"JsonPath"),state_->String(row,fields,"ConfigEntityPath")}; });
    output.players=state_->Players();
    state_->Table(output.avatars,"AvatarExcelTable","RPG.GameCore.AvatarRow",[&](void* row,const auto& fields) {
        return AvatarCatalogRow{Read<std::uint32_t>(row,Field(fields,"AvatarID","System.UInt32")),
            state_->String(row,fields,"JsonPath"),state_->String(row,fields,"DefaultAvatarModelPath")}; });
    return output;
}
TextInput OfficialCatalog::ReadAdventureIdentity(const std::string& path) {
    TextInput output; State::Scope scope(*state_);
    try { auto config=state_->Config("AdventureCharacterConfig",path,scope);
        output.value=state_->String(config,state_->metadata.ReadObjectFields(config),"FreeStyleCharacterID"); output.available=true;
    } catch(const std::exception& error) { output.error=error.what(); }
    return output;
}
PresentationConfigInput OfficialCatalog::ReadPresentationConfig(const std::string& path) {
    PresentationConfigInput output; output.path=path; State::Scope scope(*state_);
    try {
        auto config=state_->Config("CharacterConfig",path,scope); auto fields=state_->metadata.ReadObjectFields(config);
        output=state_->Presentation(config,fields,path);
    } catch(const std::exception& error) {
        output.initial_state.error=output.event_files.error=output.disabled_event_layers.error=error.what();
    }
    return output;
}
}
