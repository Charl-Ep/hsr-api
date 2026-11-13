#pragma once
#include "AttachmentFields.hpp"
#include <hksr/presentation/attachments/AttachmentScale.hpp>
#include <functional>

namespace hksr::api::detail {
struct AttachmentScaleBindings {
    Method find, local_scale, set_local_scale, game_object, get_component, add_component, destroy, self_destroy, alive;
};
inline AttachmentScaleBindings ResolveAttachmentScale(MethodResolver& m) {
    AttachmentScaleBindings out;
    out.find = m.Resolve("RPG.GameCore", "CharacterModelComponent", "GetAttachPointInst", "RPG.Client.AttachPoint",
                         {"System.String", "System.Boolean"});
    out.local_scale = m.Resolve("RPG.Client", "AttachPoint", "get_LocalScale", "UnityEngine.Vector3", {});
    out.set_local_scale =
        m.Resolve("RPG.Client", "AttachPoint", "set_LocalScale", "System.Void", {"UnityEngine.Vector3"});
    out.game_object = m.Resolve("UnityEngine", "Component", "get_gameObject", "UnityEngine.GameObject", {});
    out.get_component = m.Resolve("UnityEngine", "Component", "GetComponent", "UnityEngine.Component", {"System.Type"});
    out.add_component =
        m.Resolve("UnityEngine", "GameObject", "AddComponent", "UnityEngine.Component", {"System.Type"});
    out.destroy = m.Resolve("UnityEngine", "Object", "Destroy", "System.Void", {"UnityEngine.Object", "System.Single"},
                            MethodKind::Static);
    out.alive =
        m.Resolve("UnityEngine", "Object", "op_Implicit", "System.Boolean", {"UnityEngine.Object"}, MethodKind::Static);
    out.self_destroy = m.Resolve("", "AttachPointSet", "SelfDestroy", "System.Void", {});
    return out;
}
// Ordering preserves the original field-restoration sequence.
enum class AttachmentScaleField { Point, Destroyed, LastTarget, Origin, Constraint, Target, Persistent };
struct AttachmentScaleReflectedField {
    void* object;
    Method get, set;
};
inline AttachmentScaleReflectedField ResolveAttachmentScaleField(MethodResolver& methods, ObjectMetadata& metadata,
                                                                 ManagedObjects& objects, AttachmentScaleField key,
                                                                 const std::function<void*(ObjectResult)>& keep) {
    const char* space = "";
    const char* type = "AttachPointSet";
    const char* name = nullptr;
    switch (key) {
    case AttachmentScaleField::LastTarget:
        name = "LastTargetScale";
        break;
    case AttachmentScaleField::Target:
        name = "TargetScale";
        break;
    case AttachmentScaleField::Origin:
        name = "OriginScale";
        break;
    case AttachmentScaleField::Point:
        name = "AttachPoint";
        break;
    case AttachmentScaleField::Constraint:
        name = "ParentConstraint";
        break;
    case AttachmentScaleField::Destroyed:
        name = "IsDestroy";
        break;
    case AttachmentScaleField::Persistent:
        space = "RPG.Client";
        type = "AttachPoint";
        name = "PersistentLocalScale";
        break;
    }
    if (!name)
        throw std::invalid_argument("unknown attachment scale field");
    auto* managed_type = keep(objects.ManagedType(space, type));
    auto* managed_name = keep(objects.NewStringUtf8(name));
    ObjectResult result;
    result.call = native::Call(methods.Resolve("System", "RuntimeType", "GetField", "System.Reflection.FieldInfo",
                                               {"System.String", "System.Reflection.BindingFlags"}),
                               &result.object, managed_type, managed_name, 54);
    auto* field = keep(result);
    const auto actual = metadata.ReadObjectType(field);
    MetadataResolver resolver({actual});
    return {field,
            resolver.Resolve(actual.name_space.c_str(), actual.name.c_str(), "GetValue", "System.Object",
                             {"System.Object"}),
            resolver.Resolve(actual.name_space.c_str(), actual.name.c_str(), "SetValue", "System.Void",
                             {"System.Object", "System.Object", "System.Reflection.BindingFlags",
                              "System.Reflection.Binder", "System.Globalization.CultureInfo"})};
}
inline ObjectResult AttachmentScaleType(ManagedObjects& objects) {
    return objects.ManagedType("", "AttachPointSet");
}
inline ObjectResult AttachmentScaleVector(ManagedObjects& objects) {
    return objects.AllocateObject("UnityEngine", "Vector3");
}
inline void WriteAttachmentScaleVector(ObjectMetadata& metadata, void* box, AttachmentScaleValue value) {
    const char* names[] = {"x", "y", "z"};
    for (int i = 0; i < 3; ++i) {
        unsigned matches = 0;
        for (const auto& field : metadata.ReadObjectFields(box))
            if (field.name == names[i] && field.type == "System.Single" && field.flags_observed &&
                !(field.flags & 0x10)) {
                ++matches;
                if (!native::CopyMemoryChecked(static_cast<char*>(box) + field.offset, &value[i], sizeof(float)))
                    throw std::runtime_error("attachment scale vector write failed");
            }
        if (matches != 1)
            throw std::runtime_error("attachment scale vector layout differs");
    }
}
inline bool AttachmentScaleDestroyed(ObjectMetadata& metadata, void* component) {
    return ReadAttachmentField<bool>(metadata, component, "IsDestroy", "System.Boolean");
}
inline void* AttachmentScaleConstraint(ObjectMetadata& metadata, void* component) {
    return ReadAttachmentField<void*>(metadata, component, "ParentConstraint", "UnityEngine.Transform");
}
inline void* AttachmentScaleTransform(ObjectMetadata& metadata, void* point) {
    return ReadAttachmentField<void*>(metadata, point, "PointTransform", "UnityEngine.Transform");
}
inline bool AttachmentScaleMatches(ObjectMetadata& metadata, void* component, void* point, void* constraint,
                                   AttachmentScaleValue value) {
    return !AttachmentScaleDestroyed(metadata, component) &&
           ReadAttachmentField<void*>(metadata, component, "AttachPoint", "RPG.Client.AttachPoint") == point &&
           AttachmentScaleConstraint(metadata, component) == constraint &&
           ReadAttachmentField<AttachmentScaleValue>(metadata, component, "TargetScale", "UnityEngine.Vector3") ==
               value;
}
}
