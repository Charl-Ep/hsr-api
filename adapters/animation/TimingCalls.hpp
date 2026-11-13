#pragma once
#include <hksr/animation/ActorTiming.hpp>
#include <hksr/native/Invocation.hpp>
namespace hksr::api::detail {
inline CallResult StartActorClock(Method finite, Method infinite, void* stack, void* name,
                                  const ActorTimeScaleInput& input, int* index) {
    return input.duration
               ? native::Call(finite, index, stack, input.factor, *input.duration, name, input.fade_in, input.fade_out,
                              static_cast<void*>(nullptr), static_cast<void*>(nullptr), input.delay, false)
               : native::Call(infinite, index, stack, input.factor, name, input.fade_in, input.fade_out,
                              static_cast<void*>(nullptr), static_cast<void*>(nullptr), input.delay, false);
}
inline CallResult WriteTimingLayer(Method method, void* component, void* name, float weight, float duration) {
    return native::CallVoid(method, component, name, weight, duration);
}
}
