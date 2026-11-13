#pragma once
#include "AttachmentFields.hpp"

namespace hksr::api::detail {
struct AttachmentVisibilityBindings {
    Method setter, list, equal;
};
inline AttachmentVisibilityBindings ResolveAttachmentVisibility(MethodResolver& m) {
    AttachmentVisibilityBindings out;
    out.setter = m.Resolve("RPG.GameCore", "CharacterVisibleComponent", "SetAttachmentVisibility", "System.Boolean",
                           {"System.String", "System.Boolean", "System.Int32"});
    out.list = m.Resolve(
        "RPG.GameCore", "CharacterVisibleComponent", "get_AttachmentVisibilityList",
        "System.Collections.Generic.List<RPG.GameCore.CharacterVisibleComponent.AttachmentVisibilityInfo>", {});
    out.equal = m.Resolve("System", "String", "Equals", "System.Boolean", {"System.String", "System.String"},
                          MethodKind::Static);
    return out;
}
inline Method AttachmentVisibilitySnapshot(ObjectMetadata& metadata, void* values) {
    const auto type = metadata.ReadObjectType(values);
    MetadataResolver resolver({type});
    return resolver.Resolve(type.name_space.c_str(), type.name.c_str(), "ToArray",
                            "RPG.GameCore.CharacterVisibleComponent.AttachmentVisibilityInfo[]", {});
}
inline void* AttachmentVisibilityName(ObjectMetadata& metadata, void* item) {
    return ReadAttachmentField<void*>(metadata, item, "AttachmentName", "System.String");
}
inline bool AttachmentVisibilityValue(ObjectMetadata& metadata, void* item) {
    return ReadAttachmentField<bool>(metadata, item, "Visible", "System.Boolean");
}
}
