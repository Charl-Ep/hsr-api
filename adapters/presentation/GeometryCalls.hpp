#pragma once
#include <hksr/presentation/hits/HitGeometry.hpp>
#include <hksr/native/Invocation.hpp>
namespace hksr::api::detail {
inline CallResult HitLogicalVector(Method method, void* transform, Position3 input, Position3* output) {
    return native::Call(method, output, transform, input);
}
inline CallResult HitWorldScale(Method method, void* transform, Position3* output) {
    return native::Call(method, output, transform);
}
inline CallResult HitUnityPosition(Method method, void* transform, Position3* output) {
    return native::CallVoid(method, transform, output);
}
inline CallResult HitUnityDirection(Method method, void* transform, Position3 input, Position3* output) {
    return native::CallVoid(method, transform, &input, output);
}
inline CallResult HitUnityInversePoint(Method method, void* transform, Position3 input, Position3* output) {
    return native::CallVoid(method, transform, &input, output);
}
}
