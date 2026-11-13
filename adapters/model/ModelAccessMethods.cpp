#include <hksr/model/ModelAccess.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <stdexcept>

namespace hksr::api {
Observed<bool> ModelLoadCompleted(const ModelSnapshot& snapshot) {
    return {snapshot.load_state.value == 5, snapshot.load_state.read};
}
ModelAccessMethods ResolveModelAccessMethods(MethodResolver& source, const std::vector<DeclaredField>& fields) {
    ModelAccessMethods out;
    auto get = [&](const char* name, const char* result) {
        try {
            return source.Resolve("RPG.GameCore", "CharacterModelComponent", name, result, {});
        } catch (const std::exception& e) {
            out.binding_errors.push_back(std::string("CharacterModelComponent.") + name + ": " + e.what());
            return Method{};
        }
    };
    out.art = get("GetModelArt", "UnityEngine.GameObject");
    out.animator = get("get_Animator", "UnityEngine.Animator");
    out.load_state = get("get_LoadState", "RPG.GameCore.EntityLoadState");
    out.changing = get("get_IsChangingModel", "System.Boolean");
    out.root_loaded = get("IsModelLoad", "System.Boolean");
    out.art_loaded = get("IsArtLoaded", "System.Boolean");
    unsigned matches = 0;
    for (const auto& field : fields)
        if (field.owner_namespace == "RPG.GameCore" && field.owner_name == "CharacterModelComponent" &&
            field.name == "_unityGO" && field.type == "UnityEngine.GameObject") {
            ++matches;
            out.root_offset = field.offset;
        }
    out.has_root_field = matches == 1;
    if (!out.has_root_field)
        out.binding_errors.push_back("CharacterModelComponent._unityGO: missing or ambiguous declared field");
    return out;
}
}
