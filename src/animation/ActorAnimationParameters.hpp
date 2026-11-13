#pragma once
#include <hksr/animation/AnimationParameter.hpp>
#include <hksr/model/ModelAccess.hpp>
#include <hksr/entity/EntityAccess.hpp>
#include <hksr/entity/EntityOwner.hpp>
#include <hksr/presentation/effects/EffectRelations.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include <cmath>
#include <stdexcept>

namespace hksr::api::detail {
struct ParameterEffect {
    bool owned = false, synchronized = false;
    std::vector<void*> animators;
    std::function<bool()> current;
};
struct AnimationParameterOperations {
    std::function<bool()> current;
    std::function<CallResult(const AnimationParameter&)> body;
    std::function<std::vector<ParameterEffect>()> effects;
    std::function<CallResult(void*, const AnimationParameter&)> effect;
};
// Shared by the native adapter and local tests. No discovery or implicit target.
inline AnimationParameterResult ApplyAnimationParameter(const AnimationParameterOperations& ops,
                                                        const AnimationParameter& input,
                                                        AnimationParameterPropagation propagation) {
    AnimationParameterResult out;
    if (input.name.empty() || input.name.find('\0') != std::string::npos || input.value.valueless_by_exception() ||
        (std::holds_alternative<float>(input.value) && !std::isfinite(std::get<float>(input.value))) ||
        (propagation != AnimationParameterPropagation::BodyOnly &&
         propagation != AnimationParameterPropagation::SyncPropState)) {
        out.detail = "invalid animation parameter";
        return out;
    }
    const auto value = input;
    try {
        if (!ops.current || !ops.body) {
            out.detail = "animation parameter service unavailable";
            return out;
        }
        if (!ops.current()) {
            out.state = AnimationParameterState::OwnerChanged;
            out.detail = "actor or model ended";
            return out;
        }
        out.body_attempted = true;
        out.body = ops.body(value);
        if (!out.body.returned) {
            out.state = AnimationParameterState::Failed;
            out.detail = "body parameter write failed; application may be partial";
            return out;
        }
        if (!ops.current()) {
            out.state = AnimationParameterState::OwnerChanged;
            out.detail = "actor or model changed during body write";
            return out;
        }
        out.state = AnimationParameterState::Applied;
        if (propagation == AnimationParameterPropagation::SyncPropState) {
            if (!ops.effects || !ops.effect)
                throw std::runtime_error("SyncPropState parameter bindings unavailable");
            const auto effects = ops.effects();
            out.associated_effects = effects.size();
            for (const auto& effect : effects) {
                if (!effect.owned) {
                    ++out.foreign_effects;
                    continue;
                }
                if (!effect.synchronized) {
                    ++out.unsynchronized_effects;
                    continue;
                }
                out.effect_animators += effect.animators.size();
                for (auto* animator : effect.animators) {
                    if (!ops.current()) {
                        out.state = AnimationParameterState::OwnerChanged;
                        out.detail = "actor or model changed during propagation";
                        return out;
                    }
                    if (!effect.current || !effect.current()) {
                        ++out.ended_effects;
                        break;
                    }
                    if (!animator)
                        continue;
                    const auto call = ops.effect(animator, value);
                    if (call.returned)
                        ++out.effect_writes;
                    else
                        ++out.effect_failures;
                }
            }
            if (out.effect_failures) {
                out.state = AnimationParameterState::Partial;
                out.detail = "one or more effect parameter writes failed";
            }
        }
        out.owner_current = ops.current();
        if (!out.owner_current) {
            out.state = AnimationParameterState::OwnerChanged;
            out.detail = "actor or model changed during parameter write";
        }
    } catch (const std::exception& e) {
        out.state = out.body.returned ? AnimationParameterState::Partial : AnimationParameterState::Failed;
        out.detail = e.what();
    }
    return out;
}
class ActorAnimationParameters {
  public:
    using Source = std::function<EntityOrigin(EffectReference, const EffectRelationsSnapshot&)>;
    ActorAnimationParameters(RuntimeMetadata&, ReferenceMethods, EntityAccess, ModelAccess, EntityOwner);
    ~ActorAnimationParameters();
    AnimationParameterResult Apply(EntityToken, GameWorld*, const AnimationParameter&, AnimationParameterPropagation,
                                   const std::function<ObjectResult()>& manager, const Source&);
    const std::string& PropagationError() const;
    bool Close();

  private:
    struct State;
    std::unique_ptr<State> state_;
};
}
