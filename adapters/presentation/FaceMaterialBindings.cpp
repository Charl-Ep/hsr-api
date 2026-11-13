#include "FaceMaterialBindings.hpp"
#include <stdexcept>
namespace hksr::api::detail {
FaceMaterialBindings ResolveFaceMaterialBindings(RuntimeMetadata& metadata,
                                                 const std::function<void*(ObjectResult)>& retain) {
    FaceMaterialBindings s;
    const auto Type = [&](const char* ns, const char* name) { return retain(metadata.ManagedType(ns, name)); };
    s.face_type = Type("", "DEGDOEMPKCM");
    s.model_type = Type("RPG.GameCore", "CharacterModelComponent");
    s.art_loaded = metadata.Resolve("RPG.GameCore", "CharacterModelComponent", "IsArtLoaded", "System.Boolean", {});
    s.on_art_loaded = metadata.Resolve("", "DEGDOEMPKCM", "OnModelArtLoaded", "System.Void", {});
    s.component_owner =
        metadata.Resolve("RPG.GameCore", "GameComponentBase", "get_OwnerRef", "RPG.GameCore.GameEntity", {});
    unsigned matches = 0;
    for (const auto& field : metadata.ReadFields("", "DEGDOEMPKCM", false))
        if (field.name == "EAIBBECPIFJ" && field.type == "RPG.Client.RendererMaterialCache" && field.flags_observed &&
            !(field.flags & 0x10)) {
            s.cache_offset = field.offset;
            ++matches;
        }
    if (matches != 1)
        throw std::runtime_error("face material cache declaration differs");
    s.entity_type = Type("RPG.GameCore", "GameEntity");
    return s;
}
}
