#pragma once
#include <hksr/animation/AnimationParameter.hpp>
#include <hksr/native/Invocation.hpp>
#include <type_traits>

namespace hksr::api::detail {
struct AnimationParameterSetters {
    Method floating, integer, boolean, trigger, reset_trigger;
};
inline AnimationParameterSetters ResolveAnimationParameterSetters(MethodResolver& metadata, const char* space,
                                                                  const char* type) {
    return {metadata.Resolve(space, type, "SetFloat", "System.Void", {"System.String", "System.Single"}),
            metadata.Resolve(space, type, "SetInteger", "System.Void", {"System.String", "System.Int32"}),
            metadata.Resolve(space, type, "SetBool", "System.Void", {"System.String", "System.Boolean"}),
            metadata.Resolve(space, type, "SetTrigger", "System.Void", {"System.String"}),
            metadata.Resolve(space, type, "ResetTrigger", "System.Void", {"System.String"})};
}
inline CallResult WriteAnimationParameter(const AnimationParameterSetters& methods, void* receiver, void* name,
                                          const AnimationParameter& input) {
    if (!receiver || !name || input.value.valueless_by_exception())
        return {};
    return std::visit(
        [&](const auto& value) -> CallResult {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, float>)
                return native::CallVoid(methods.floating, receiver, name, value);
            else if constexpr (std::is_same_v<T, int>)
                return native::CallVoid(methods.integer, receiver, name, value);
            else if constexpr (std::is_same_v<T, bool>)
                return native::CallVoid(methods.boolean, receiver, name, value);
            else
                return native::CallVoid(value.reset ? methods.reset_trigger : methods.trigger, receiver, name);
        },
        input.value);
}
}
