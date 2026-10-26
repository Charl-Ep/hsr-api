#pragma once
#include "EffectControl.hpp"
#include <optional>

namespace hksr::sdk_v1 {
enum class EffectMutexKind : std::int32_t { None=0, Default=1, FadeOut=2 };
enum class EffectMutexScope : std::int32_t { Global=0, Entity=1 };
struct EffectMutexRule {
    float interval=0;
    std::int32_t maximum=0;
    EffectMutexScope scope=EffectMutexScope::Entity;
};
struct HitEffectRules {
    bool force_entity=false;
    std::optional<EffectMutexRule> configured;
};
struct EffectMutexDecision {
    Observed<EffectMutexKind> kind;
    // A candidate is returned only when it is one of the supplied owned
    // receipts. Shared official effects can influence counts, never be adopted.
    std::optional<EffectReference> replay;
    std::string error;
};
}
