#include "EffectInputBindings.hpp"
#include <stdexcept>
namespace hksr::api::detail {
EffectInputFieldIdentity DescribeEffectInputField(EffectInputField field) {
    const char* name = nullptr;
    switch (field) {
    case EffectInputField::Manager:
        return {"GlobalVars", "s_MonoEffectManager", true};
    case EffectInputField::Caster:
        name = "CasterEntity";
        break;
    case EffectInputField::Target:
        name = "TargetEntity";
        break;
    case EffectInputField::ParamEntity:
        name = "ParamEntity";
        break;
    case EffectInputField::Completion:
        name = "CompleteCallback";
        break;
    case EffectInputField::Flags:
        name = "Flags";
        break;
    case EffectInputField::UniqueName:
        name = "UniqueEffectName";
        break;
    case EffectInputField::AttachPoint:
        name = "DynamicAttach";
        break;
    case EffectInputField::PositionOffset:
        name = "PositionOffset";
        break;
    case EffectInputField::RotationOffset:
        name = "RotationOffset";
        break;
    case EffectInputField::Scale:
        name = "CustomScale";
        break;
    case EffectInputField::FadeIn:
        name = "IsNeedFadeIn";
        break;
    case EffectInputField::SimulateImmediately:
        name = "ForceSimulateImmediately";
        break;
    case EffectInputField::Replay:
        name = "IsEnableReplay";
        break;
    case EffectInputField::ParamUsage:
        name = "ParamEntityUsage";
        break;
    case EffectInputField::MaxPitch:
        name = "TowardMaxPitchAngle";
        break;
    case EffectInputField::InitialPosition:
        name = "InitPosition";
        break;
    case EffectInputField::AnchorPosition:
        name = "AnchorPosition";
        break;
    case EffectInputField::InitialRotation:
        name = "InitRotation";
        break;
    case EffectInputField::IgnorePrefabAttachment:
        name = "IgnorePrefabAttachPoint";
        break;
    }
    if (!name)
        throw std::invalid_argument("unknown effect input field");
    return {"TriggerEffectParams", name, false};
}
namespace {
const char* ScalarType(EffectInputField field) {
    switch (field) {
    case EffectInputField::PositionOffset:
    case EffectInputField::RotationOffset:
    case EffectInputField::Scale:
    case EffectInputField::InitialPosition:
    case EffectInputField::AnchorPosition:
        return "System.Nullable<UnityEngine.Vector3>";
    case EffectInputField::InitialRotation:
        return "System.Nullable<UnityEngine.Quaternion>";
    case EffectInputField::FadeIn:
    case EffectInputField::SimulateImmediately:
    case EffectInputField::Replay:
    case EffectInputField::IgnorePrefabAttachment:
        return "System.Boolean";
    case EffectInputField::ParamUsage:
        return "RPG.GameCore.EffectParamEntityUsage";
    case EffectInputField::MaxPitch:
        return "System.Single";
    default:
        throw std::invalid_argument("effect input is not a blittable field");
    }
}
}
EffectInputFields ResolveEffectInputFields(ObjectMetadata& metadata, void* parameters,
                                           std::initializer_list<EffectInputField> requested) {
    const auto type = metadata.ReadObjectTypeName(parameters);
    bool compatible = type.name_space == "RPG.Client" && type.name == "TriggerEffectParams";
    if (!compatible)
        if (auto* hierarchy = dynamic_cast<TypeRelationships*>(&metadata))
            compatible = hierarchy->TypeDerivesFrom(type.name_space.c_str(), type.name.c_str(), "RPG.Client",
                                                    "TriggerEffectParams");
    if (!compatible)
        throw std::invalid_argument("entity effect parameters type differs");
    const auto fields = metadata.ReadObjectFields(parameters);
    EffectInputFields result;
    for (const auto id : requested) {
        const auto identity = DescribeEffectInputField(id);
        const auto* expected = ScalarType(id);
        const DeclaredField* found = nullptr;
        for (const auto& field : fields)
            if (field.owner_namespace == "RPG.Client" && field.owner_name == identity.owner &&
                field.name == identity.name) {
                if (found || field.type != expected || !field.flags_observed || (field.flags & 16))
                    throw std::runtime_error(std::string("entity effect field differs: ") + identity.name);
                found = &field;
            }
        if (!found)
            throw std::runtime_error(std::string("entity effect field absent: ") + identity.name);
        result.emplace(id, *found);
    }
    return result;
}
ObjectResult ResolveEffectInputOwnerType(ManagedObjects& objects, EffectInputField field) {
    return objects.ManagedType("RPG.Client", DescribeEffectInputField(field).owner);
}
ObjectResult ResolveEffectFlagType(ManagedObjects& objects) {
    return objects.ManagedType("RPG.GameCore", "MonoEffectFlag");
}
Method ResolveEffectAttachmentSetter(MethodResolver& methods) {
    return methods.Resolve("RPG.Client", "TriggerEffectParams", "set_IsAttachToTargetEntity", "System.Void",
                           {"System.Boolean"});
}
Method ResolveEffectLookRotation(MethodResolver& methods) {
    return methods.Resolve("UnityEngine", "Quaternion", "LookRotation_Injected", "System.Void",
                           {"UnityEngine.Vector3&", "UnityEngine.Vector3&", "UnityEngine.Quaternion&"},
                           MethodKind::Static);
}
}
