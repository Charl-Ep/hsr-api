#include <hksr/world/WorldPlacement.hpp>
#include <stdexcept>
#include <string>

namespace hksr::api {
WorldPlacementMethods ResolveWorldPlacementMethods(MethodResolver& source) {
    WorldPlacementMethods out;
    auto get = [&](const char* ns, const char* type, const char* name, const char* result,
                   std::initializer_list<const char*> parameters = std::initializer_list<const char*>{}) {
        try {
            return source.Resolve(ns, type, name, result, parameters);
        } catch (const std::exception& e) {
            out.binding_errors.push_back(std::string(type) + "." + name + ": " + e.what());
            return Method{};
        }
    };
    out.player = get("RPG.Client", "AdventurePhase", "get_CurrentLocalPlayer", "RPG.GameCore.GameEntity");
    out.owner_world = get("RPG.GameCore", "GameEntity", "get_OwnerWorldRef", "RPG.GameCore.GameWorld");
    out.game_object = get("RPG.GameCore", "GameEntity", "get_UnityGO", "UnityEngine.GameObject");
    out.transform = get("UnityEngine", "GameObject", "get_transform", "UnityEngine.Transform");
    out.position = get("UnityEngine", "Transform", "get_position_Injected", "System.Void", {"UnityEngine.Vector3&"});
    out.rotation = get("UnityEngine", "Transform", "get_rotation_Injected", "System.Void", {"UnityEngine.Quaternion&"});
    return out;
}
}
