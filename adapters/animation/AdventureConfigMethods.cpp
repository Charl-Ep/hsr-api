#include <hksr/animation/AdventureConfig.hpp>
#include "../../src/metadata/ResolveBatch.hpp"
namespace hksr::sdk_v1 {
AdventureDataField ResolveAdventureDataField(const std::vector<DeclaredField>& fields) {
    AdventureDataField out; unsigned matches=0;
    for(const auto& field:fields) if(field.type=="RPG.GameCore.AdventureCharacterConfig") {
        if(!field.flags_observed) {
            out.binding_errors.push_back("AdventureCharacterDataComponent config field flags unavailable");
            continue;
        }
        if(field.flags&0x10) continue;
        ++matches; out.offset=field.offset;
    }
    out.available=matches==1 && out.binding_errors.empty();
    if(matches!=1) out.binding_errors.push_back("AdventureCharacterDataComponent config instance field missing or ambiguous");
    return out;
}
AdventureConfigMethods ResolveAdventureConfigMethods(MethodResolver& source,const std::vector<DeclaredField>& fields) {
    detail::ResolveBatch b(source); AdventureConfigMethods out;
    out.load=b.Resolve("RPG.GameCore","GameCoreConfigLoader","LoadAdventureCharacterConfig",
        "RPG.GameCore.AdventureCharacterConfig",{"System.String"},MethodKind::Static);
    out.binding_errors=b.Errors();
    unsigned weapons=0,events=0;
    for(const auto& field:fields) if(field.type=="System.String[]") {
        if(field.name=="HideWeaponList") { ++weapons; out.hidden_weapons_offset=field.offset; }
        if(field.name=="AnimEventConfigList") { ++events; out.event_paths_offset=field.offset; }
    }
    out.has_hidden_weapons=weapons==1; out.has_event_paths=events==1;
    if(!out.has_hidden_weapons) out.binding_errors.push_back("AdventureCharacterConfig.HideWeaponList missing or ambiguous");
    if(!out.has_event_paths) out.binding_errors.push_back("AdventureCharacterConfig.AnimEventConfigList missing or ambiguous");
    return out;
}
}
