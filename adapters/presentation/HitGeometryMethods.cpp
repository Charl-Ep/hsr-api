#include <hksr/presentation/hits/HitGeometry.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <stdexcept>
namespace hksr::api {
HitGeometryMethods ResolveHitGeometryMethods(MethodResolver& methods, const std::vector<DeclaredField>& fields) {
    HitGeometryMethods out;
    auto get = [&](const char* ns, const char* type, const char* name, const char* result,
                   std::initializer_list<const char*> parameters) {
        try {
            return methods.Resolve(ns, type, name, result, parameters);
        } catch (const std::exception& e) {
            out.binding_errors.push_back(std::string(type) + "." + name + ": " + e.what());
            return Method{};
        }
    };
    out.transform_vector =
        get("RPG.GameCore", "TransformComponent", "TransformVector", "RPG.MVector3", {"RPG.MVector3"});
    out.world_scale = get("RPG.GameCore", "TransformComponent", "get_WorldScale", "RPG.MVector3", {});
    out.position = get("UnityEngine", "Transform", "get_position_Injected", "System.Void", {"UnityEngine.Vector3&"});
    out.direction = get("UnityEngine", "Transform", "TransformDirection_Injected", "System.Void",
                        {"UnityEngine.Vector3&", "UnityEngine.Vector3&"});
    out.inverse_point = get("UnityEngine", "Transform", "InverseTransformPoint_Injected", "System.Void",
                            {"UnityEngine.Vector3&", "UnityEngine.Vector3&"});
    out.attach_point = get("RPG.GameCore", "CharacterModelComponent", "GetAttachPoint", "UnityEngine.Transform",
                           {"System.String", "System.Boolean"});
    unsigned matches = 0;
    for (const auto& f : fields)
        if (f.owner_namespace == "RPG.GameCore" && f.owner_name == "TransformComponent" && f.name == "_WorldPos" &&
            f.type == "RPG.MVector3" && f.flags_observed && !(f.flags & 0x10)) {
            ++matches;
            out.logical_position_offset = f.offset;
        }
    out.logical_position_available = matches == 1;
    if (!out.logical_position_available)
        out.binding_errors.push_back("TransformComponent._WorldPos: missing or ambiguous field");
    return out;
}
}
