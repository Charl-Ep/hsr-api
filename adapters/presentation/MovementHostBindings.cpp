#include "MovementHostBindings.hpp"
#include <optional>
#include <stdexcept>
namespace hksr::api::detail {
namespace {
std::size_t Field(RuntimeMetadata& m, const char* space, const char* type, const char* name, const char* value_type) {
    std::optional<std::size_t> out;
    for (const auto& f : m.ReadFields(space, type))
        if (f.name == name) {
            if (out || f.type != value_type || !f.flags_observed || (f.flags & 0x10))
                throw std::runtime_error(std::string("movement field differs: ") + type + "." + name);
            out = f.offset;
        }
    if (!out)
        throw std::runtime_error(std::string("movement field absent: ") + type + "." + name);
    return *out;
}
}
MovementHostBindings ResolveMovementHostBindings(RuntimeMetadata& m, const std::function<void*(ObjectResult)>& retain) {
    MovementHostBindings out;
    const auto Type = [&](const char* ns, const char* name) { return retain(m.ManagedType(ns, name)); };
    out.component =
        m.Resolve("RPG.GameCore", "GameEntity", "GetComponent", "RPG.GameCore.GameComponentBase", {"System.Type"});
    out.destroy = m.Resolve("RPG.GameCore", "GameEntity", "DestroyComponent", "System.Void", {"System.Type"});
    out.initialize = m.Resolve(
        "", "BGOBCKLPEMK", "LMCEDCHJKFF", "System.Void",
        {"IBEJMEEEJMB", "RPG.GameCore.TransformComponent", "RPG.GameCore.CharacterDataComponent", "ODAKFODAONI"});
    out.root_get =
        m.Resolve("RPG.GameCore", "AdventureCharacterController", "get_EnableRootMotion", "System.Boolean", {});
    out.root_set = m.Resolve("RPG.GameCore", "AdventureCharacterController", "set_EnableRootMotion", "System.Void",
                             {"System.Boolean"});
    out.callback_get = m.Resolve("", "BGOBCKLPEMK", "get_EnableAnimatorMoveCallback", "System.Boolean", {});
    out.callback_set =
        m.Resolve("", "BGOBCKLPEMK", "set_EnableAnimatorMoveCallback", "System.Void", {"System.Boolean"});
    out.animator_get =
        m.Resolve("RPG.GameCore", "AdventureCharacterController", "GetUnityAnimator", "UnityEngine.Animator", {});
    out.is_instance = m.Resolve("System", "Type", "IsInstanceOfType", "System.Boolean", {"System.Object"});
    out.external_root = Field(m, "RPG.GameCore", "AdventureCharacterController", "EnableRootMotionExternal",
                              "System.Func<System.Boolean>");
    out.external_move =
        Field(m, "RPG.GameCore", "AdventureCharacterController", "<ExternalMoveHandler>k__BackingField", "MHHGLBGPCFH");
    out.proxy = Field(m, "RPG.GameCore", "AdventureCharacterController", "_MovementProxy", "KMEOMKNPNCC");
    out.thread_data = Field(m, "", "KMEOMKNPNCC", "<ThreadData>k__BackingField", "FEIBCGELFAJ");
    out.pending = Field(m, "", "FEIBCGELFAJ", "LCEIFCIKKAN", "System.Boolean");
    out.access = ResolveAnimationMovementAccess(m);
    // The unique declared zero-argument factory is verified before name-
    // based reflection. Never select a runtime code address by RVA here.
    unsigned matched = 0;
    for (const auto& method : m.ReadType("RPG.GameCore", "GameEntity").methods)
        if (method.name == "CreateComponent") {
            if (!method.parameters.empty() || method.is_static || method.result != "T")
                throw std::runtime_error("CreateComponent declaration changed");
            ++matched;
        }
    if (matched != 1)
        throw std::runtime_error("CreateComponent declaration is not unique");
    out.movement_type = Type("", "BGOBCKLPEMK");
    out.adventure_type = Type("RPG.GameCore", "AdventureCharacterController");
    out.transform_type = Type("RPG.GameCore", "TransformComponent");
    out.data_type = Type("RPG.GameCore", "CharacterDataComponent");
    out.provider_type = Type("", "IBEJMEEEJMB");
    out.entity_type = Type("RPG.GameCore", "GameEntity");
    return out;
}
}
