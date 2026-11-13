#include "ControlNativeBindings.hpp"
#include <hksr/control/MouseMode.hpp>
#include <stdexcept>

namespace hksr::api {
ActorControlMethods ResolveActorControlMethods(MethodResolver& source) {
    ActorControlMethods out;
    auto get = [&](const char* ns, const char* type, const char* name, const char* result,
                   std::initializer_list<const char*> params = std::initializer_list<const char*>{}) {
        try {
            return source.Resolve(ns, type, name, result, params);
        } catch (const std::exception& e) {
            out.binding_errors.push_back(std::string(type) + "." + name + ": " + e.what());
            return Method{};
        }
    };
    out.flush = get("RPG.GameCore", "AdventureCharacterController", "FlushMovementThreadData", "System.Void");
    out.place = get("RPG.GameCore", "AdventureCharacterController", "SetCharacterPositionAndRotation", "System.Void",
                    {"UnityEngine.Vector3", "UnityEngine.Quaternion"});
    out.mono = get("RPG.GameCore", "AdventureCharacterController", "get_MonoAvatar", "RPG.Client.CharacterMonoAvatar");
    out.look_enabled = get("", "PKBOEALAKCM", "CIMKMBDNGIK", "System.Boolean");
    out.set_look_enabled = get("", "PKBOEALAKCM", "IDCGHHPJLGF", "System.Void", {"System.Boolean"});
    out.head = get("RPG.Client", "CharacterMonoAvatar", "get_Head", "UnityEngine.Transform");
    out.look = get("RPG.Client", "CharacterMonoAvatar", "get_LookAtIK", "RPG.Client.LookAtIK");
    out.follow =
        get("RPG.Client", "CharacterMonoAvatar", "HeadLookAt", "System.Void",
            {"UnityEngine.Transform", "System.Boolean", "System.Boolean", "RPG.GameCore.CharacterHeadConstraint"});
    out.stop = get("RPG.Client", "CharacterMonoAvatar", "HeadStopLookAt", "System.Void", {"System.Boolean"});
    out.point = get("UnityEngine", "Transform", "set_position_Injected", "System.Void", {"UnityEngine.Vector3&"});
    out.model_attach_point = get("RPG.GameCore", "CharacterModelComponent", "GetAttachPoint", "UnityEngine.Transform",
                                 {"System.String", "System.Boolean"});
    out.forbid_ground = get("RPG.GameCore", "AdventureCharacterController", "set_ForbidStandOnGround", "System.Void",
                            {"System.Boolean"});
    out.get_forbid_ground =
        get("RPG.GameCore", "AdventureCharacterController", "get_ForbidStandOnGround", "System.Boolean");
    out.navigation_only = get("RPG.GameCore", "AdventureCharacterController", "set_UseNavigationMotionOnly",
                              "System.Void", {"System.Boolean"});
    out.get_navigation_only =
        get("RPG.GameCore", "AdventureCharacterController", "get_UseNavigationMotionOnly", "System.Boolean");
    out.navigation_ground = get("RPG.GameCore", "AdventureCharacterController", "set_UseNavigationToGround",
                                "System.Void", {"System.Boolean"});
    out.get_navigation_ground =
        get("RPG.GameCore", "AdventureCharacterController", "get_UseNavigationToGround", "System.Boolean");
    out.ik_head = get("RPG.Client", "LookAtIK", "get_Head", "UnityEngine.Transform");
    out.ik_follow =
        get("RPG.Client", "LookAtIK", "HeadLookAt", "System.Void",
            {"UnityEngine.Transform", "System.Boolean", "System.Boolean", "RPG.GameCore.CharacterHeadConstraint"});
    out.ik_stop = get("RPG.Client", "LookAtIK", "HeadStopLookAt", "System.Void", {"System.Boolean"});
    out.ik_active = get("RPG.Client", "LookAtIK", "get_IsActive", "System.Boolean");
    out.behaviour_enabled = get("UnityEngine", "Behaviour", "get_enabled", "System.Boolean");
    out.set_behaviour_enabled = get("UnityEngine", "Behaviour", "set_enabled", "System.Void", {"System.Boolean"});
    out.yaw = get("RPG.GameCore", "TransformComponent", "get_YawAngle", "System.Single");
    out.turn = get("RPG.GameCore", "TransformComponent", "SmoothChangeYawAngleStart", "System.Void",
                   {"System.Single", "System.Single", "System.Boolean"});
    return out;
}
MouseModeMethods ResolveMouseMode(RuntimeMetadata& m) {
    MouseModeMethods out;
    out.instance = m.Resolve("RPG.Client", "MouseInputController", "get_Instance", "RPG.Client.MouseInputController",
                             {}, MethodKind::Static);
    out.force = m.Resolve("RPG.Client", "MouseInputController", "ForceShowCursor", "System.Void", {});
    out.force_sdk = m.Resolve("RPG.Client", "MouseInputController", "ForceShowCursorBySDKUI", "System.Void", {});
    out.close = m.Resolve("RPG.Client", "MouseInputController", "CloseForceShowCursor", "System.Void", {});
    out.hide = m.Resolve("RPG.Client", "MouseInputController", "HideCursor", "System.Void", {});
    out.refresh = m.Resolve("RPG.Client", "MouseInputController", "_ShowCursor", "System.Void", {});
    out.use_mode = m.Resolve("RPG.Client", "MouseInputController", "get_UseCursorMode", "System.Boolean", {});
    out.visible = m.Resolve("UnityEngine", "Cursor", "get_visible", "System.Boolean", {}, MethodKind::Static);
    out.lock =
        m.Resolve("UnityEngine", "Cursor", "get_lockState", "UnityEngine.CursorLockMode", {}, MethodKind::Static);
    for (const auto& f : m.ReadFields("RPG.Client", "MouseInputController", false))
        if (f.name == "_ForceShowCursor" && f.type == "System.Boolean" && f.flags_observed && !(f.flags & 0x10))
            out.forced_offset = f.offset;
    if (!out.forced_offset)
        throw std::runtime_error("official cursor ownership field unavailable");
    return out;
}
}

namespace hksr::api::detail {
ObjectResult ResolveControlType(ManagedObjects& objects, ControlType type) {
    switch (type) {
    case ControlType::AdventureController:
        return objects.ManagedType("RPG.GameCore", "AdventureCharacterController");
    case ControlType::Model:
        return objects.ManagedType("RPG.GameCore", "CharacterModelComponent");
    case ControlType::GazeDriver:
        return objects.ManagedType("", "PKBOEALAKCM");
    case ControlType::LookIk:
        return objects.ManagedType("RPG.Client", "LookAtIK");
    case ControlType::AlternateLookIk:
        return objects.ManagedType("RPG.Client", "ZatzkaLookAtIK");
    case ControlType::SceneObject:
        return objects.ManagedType("UnityEngine", "GameObject");
    case ControlType::Transform:
        return objects.ManagedType("UnityEngine", "Transform");
    }
    throw std::invalid_argument("unknown control type");
}
ObjectResult ControlHeadName(ManagedObjects& objects) {
    return objects.NewStringUtf8("Head");
}
ObjectResult AllocateControlPoint(ManagedObjects& objects) {
    return objects.AllocateObject("UnityEngine", "GameObject");
}
ActorControlMethods BindControlMono(const ActorControlMethods& methods, ObjectMetadata& metadata, void* mono) {
    const auto type = metadata.ReadObjectType(mono);
    MetadataResolver resolver({type});
    auto updated = methods;
    // Virtual getters may be abstract on CharacterMonoAvatar. Prefer this
    // actual model's implementation; do not call an abstract base slot.
    auto get = [&](Method& method, const char* name, const char* result) {
        bool declared = false;
        for (const auto& entry : type.methods)
            if (entry.name == name && entry.parameters.empty())
                declared = true;
        if (declared)
            method = resolver.Resolve(type.name_space.c_str(), type.name.c_str(), name, result, {});
    };
    get(updated.head, "get_Head", "UnityEngine.Transform");
    get(updated.look, "get_LookAtIK", "RPG.Client.LookAtIK");
    return updated;
}
}
