#pragma once
#include <hksr/presentation/effects/EffectActions.hpp>
#include <hksr/native/Invocation.hpp>
#include <limits>
namespace hksr::api::detail {
inline CallResult WriteEffectAnimatorState(Method play, Method fade, void* animator, int state, float duration,
                                           bool restart) {
    const float start = restart ? 0.f : -std::numeric_limits<float>::infinity();
    return duration > 0 ? native::CallVoid(fade, animator, state, duration, 0, start, 0.f)
                        : native::CallVoid(play, animator, state, 0, start);
}
inline CallResult AcquireEffectRemoval(Method method, void** output, std::uint32_t id,
                                       const EffectRemovalOptions& options) {
    return native::Call(method, output, static_cast<void*>(nullptr), static_cast<void*>(nullptr),
                        static_cast<void*>(nullptr), static_cast<void*>(nullptr), options.unbind,
                        options.immediate_fade, id, static_cast<void*>(nullptr), true);
}
}
