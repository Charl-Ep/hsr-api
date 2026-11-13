#pragma once
#include "AnimationParameterCalls.hpp"
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/managed/ManagedLists.hpp>
#include <hksr/presentation/effects/EffectRelations.hpp>
#include <functional>
#include <optional>
namespace hksr::api::detail {
struct AnimationParameterBindings {
    void* model_type = nullptr;
    Method mock, mock_animator, find_effects, behavior, animators;
    AnimationParameterSetters body, effect;
    std::optional<ManagedReferenceList> effect_list, animator_list;
    std::optional<EffectRelations> relations;
    std::size_t request_offset = 0, id_offset = 0, sync_offset = 0;
};
using AnimationParameterRoot = std::function<void(ObjectResult)>;
void ResolveAnimationParameterBody(RuntimeMetadata&, AnimationParameterBindings&, const AnimationParameterRoot&);
void ResolveAnimationParameterEffects(RuntimeMetadata&, AnimationParameterBindings&);
}
