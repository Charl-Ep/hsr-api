#include <hksr/animation/HitDirection.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <stdexcept>
#include <string>

namespace hksr::api {
HitDirectionMethods ResolveHitDirectionMethods(MethodResolver& source) {
    HitDirectionMethods out;
    auto get = [&](const char* type, const char* name, std::initializer_list<const char*> parameters, MethodKind kind) {
        try {
            return source.Resolve("UnityEngine", type, name, "System.Void", parameters, kind);
        } catch (const std::exception& e) {
            out.binding_errors.push_back(std::string(type) + "." + name + ": " + e.what());
            return Method{};
        }
    };
    out.inverse_direction = get("Transform", "InverseTransformDirection_Injected",
                                {"UnityEngine.Vector3&", "UnityEngine.Vector3&"}, MethodKind::Instance);
    out.rotation = get("Transform", "get_rotation_Injected", {"UnityEngine.Quaternion&"}, MethodKind::Instance);
    out.look_rotation =
        get("Quaternion", "LookRotation_Injected",
            {"UnityEngine.Vector3&", "UnityEngine.Vector3&", "UnityEngine.Quaternion&"}, MethodKind::Static);
    out.to_euler = get("Quaternion", "Internal_ToEulerRad_Injected",
                       {"UnityEngine.Quaternion&", "UnityEngine.Vector3&"}, MethodKind::Static);
    try {
        out.calculate_world = source.Resolve("RPG.GameCore", "AbilityStatic", "_CalcHitDir", "RPG.MVector3",
                                             {"RPG.MVector3", "System.Single", "System.Single"}, MethodKind::Static);
    } catch (const std::exception& e) {
        out.binding_errors.push_back(std::string("AbilityStatic._CalcHitDir: ") + e.what());
    }
    return out;
}
void ResolveHitDirectionModel(HitDirectionMethods& methods, const std::vector<DeclaredField>& fields) {
    unsigned count = 0;
    for (const auto& f : fields)
        if (f.owner_namespace == "RPG.GameCore" && f.owner_name == "CharacterModelComponent" &&
            f.name == "_ModelFlip" && f.type == "System.Boolean" && f.flags_observed && !(f.flags & 0x10)) {
            ++count;
            methods.mirror_offset = f.offset;
        }
    methods.mirror_available = count == 1;
    if (!methods.mirror_available)
        methods.binding_errors.push_back("CharacterModelComponent._ModelFlip: missing or ambiguous field");
}
}
