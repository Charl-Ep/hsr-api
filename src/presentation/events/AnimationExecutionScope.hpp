#pragma once
#include <hksr/presentation/events/AnimationSequenceExecutions.hpp>
namespace hksr::sdk_v1::detail {
// Opaque stack receipt; valid only until the original sequence call returns.
void* ExchangeAnimationExecutionScope(void*) noexcept;
void* CurrentAnimationExecutionScope() noexcept;
AnimationSequenceExecution SequenceFromScope(void*,EntityOrigin) noexcept;
}
