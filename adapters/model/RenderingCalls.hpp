#pragma once
#include <hksr/native/Invocation.hpp>
#include <cstddef>
namespace hksr::api::detail {
struct RenderingVisibilityInput {
    void* key = nullptr;
    bool collider = false, trigger = false, audio = false;
    unsigned char padding[5]{};
};
static_assert(sizeof(RenderingVisibilityInput) == 16 && offsetof(RenderingVisibilityInput, collider) == 8 &&
              offsetof(RenderingVisibilityInput, audio) == 10);
inline CallResult SetRenderingVisibility(Method method, void* component, void* rooted_key, bool visible) {
    RenderingVisibilityInput input;
    input.key = rooted_key;
    return native::CallVoid(method, component, visible, &input);
}
inline CallResult SetRenderingMask(Method method, void* renderer, std::uint32_t mask) {
    return native::CallVoid(method, renderer, mask);
}
inline CallResult SetRenderingOutline(Method method, void* effect, void* rooted_add, void* rooted_remove) {
    return native::CallVoid(method, effect, true, rooted_add, rooted_remove);
}
}
