#include <hksr/entity/EntityAccess.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
Observed<bool> ModelLoadCompleted(const ModelSnapshot& snapshot) {
    return {snapshot.load_state.value==5,snapshot.load_state.read};
}
namespace {
Method ReadBinding(MethodResolver& source, std::vector<std::string>& errors, const char* owner,
    const char* name, const char* result, std::initializer_list<const char*> parameters = {}) {
    try { return source.Resolve("RPG.GameCore", owner, name, result, parameters); }
    catch (const std::exception& e) { errors.push_back(std::string(owner) + "." + name + ": " + e.what()); return {}; }
}
}
EntityAccessMethods ResolveEntityAccessMethods(MethodResolver& source) {
    EntityAccessMethods out;
    auto& e = out.binding_errors;
    out.runtime_id = ReadBinding(source, e, "GameEntity", "get_RuntimeID", "System.UInt32");
    out.owner_world = ReadBinding(source, e, "GameEntity", "get_OwnerWorldRef", "RPG.GameCore.GameWorld");
    out.loaded = ReadBinding(source, e, "GameEntity", "get_IsLoaded", "System.Boolean");
    out.entity_type = ReadBinding(source, e, "GameEntity", "get_EntityType", "RPG.GameCore.EntityType");
    out.alive_state = ReadBinding(source, e, "GameEntity", "get_StateAlive", "RPG.GameCore.AliveState");
    out.component = ReadBinding(source, e, "GameEntity", "GetComponent", "RPG.GameCore.GameComponentBase", {"System.Type"});
    out.manager_lookup = ReadBinding(source, e, "EntityManager", "GetEntityByRuntimeID", "RPG.GameCore.GameEntity", {"System.UInt32"});
    out.kill_immediately = ReadBinding(source, e, "GameEntity", "set_KillImmediately", "System.Void", {"System.Boolean"});
    out.make_die = ReadBinding(source, e, "GameWorld", "MakeEntityDie", "System.Void", {"RPG.GameCore.GameEntity", "RPG.GameCore.GameEntity"});
    return out;
}
ModelAccessMethods ResolveModelAccessMethods(MethodResolver& source, const std::vector<DeclaredField>& fields) {
    ModelAccessMethods out;
    auto get = [&](const char* name, const char* result) { return ReadBinding(source, out.binding_errors, "CharacterModelComponent", name, result); };
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
            ++matches; out.root_offset = field.offset;
        }
    out.has_root_field = matches == 1;
    if (!out.has_root_field) out.binding_errors.push_back("CharacterModelComponent._unityGO: missing or ambiguous declared field");
    return out;
}
}
