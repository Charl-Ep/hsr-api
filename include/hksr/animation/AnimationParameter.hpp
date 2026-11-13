#pragma once
#include "../metadata/MethodResolver.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>

namespace hksr::api {
struct AnimationTrigger {
    bool reset = false;
};
struct AnimationParameter {
    std::string name;
    std::variant<float, int, bool, AnimationTrigger> value = 0.0f;
};
enum class AnimationParameterPropagation { BodyOnly, SyncPropState };
enum class AnimationParameterState { Unavailable, Applied, Partial, OwnerChanged, Failed };
// Synchronous command receipts, not animation completion or a restoration lease.
// Effects are the actor's current SyncPropState requests, not future effects.
struct AnimationParameterResult {
    AnimationParameterState state = AnimationParameterState::Unavailable;
    CallResult body;
    bool body_attempted = false, owner_current = false, references_released = true;
    std::uintptr_t model_identity = 0; // comparison only; never dereference
    std::size_t associated_effects = 0, foreign_effects = 0, unsynchronized_effects = 0;
    std::size_t ended_effects = 0, effect_animators = 0, effect_writes = 0, effect_failures = 0;
    std::string detail;
};
}
