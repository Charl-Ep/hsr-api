#pragma once
#include <hksr/presentation/events/AnimationEventPolicy.hpp>

namespace hksr::api::detail {
inline std::optional<AnimationStringKind> EventStringField(const DeclaredField& field) {
    if (field.owner_namespace != "RPG.GameCore" || field.type != "System.String")
        return {};
    if (((field.owner_name == "TriggerEffect" || field.owner_name == "RemoveEffect") && field.name == "EffectPath") ||
        (field.owner_name == "AttachmentVisibilityNode" && field.name == "TriggerEffectPath"))
        return AnimationStringKind::ResourcePath;
    if (field.owner_name == "TriggerSoundInAnim" && field.name == "SoundName")
        return AnimationStringKind::AnimationSound;
    return {};
}
}
