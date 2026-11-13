#pragma once
#include <hksr/presentation/events/AnimationSequenceExecutions.hpp>
namespace hksr::api::detail {
// Opaque stack receipt; valid only until the original sequence call returns.
void* ExchangeAnimationExecutionScope(void*) noexcept;
void* CurrentAnimationExecutionScope() noexcept;
AnimationSequenceExecution SequenceFromScope(void*, EntityOrigin) noexcept;
}
