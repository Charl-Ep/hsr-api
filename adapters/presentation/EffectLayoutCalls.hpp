#pragma once
#include <hksr/native/Invocation.hpp>
namespace hksr::api::detail {
inline CallResult LayoutPerform(Method method, void* behavior, bool max_highlight) {
    return max_highlight ? native::CallVoid(method, behavior, false) : native::CallVoid(method, behavior);
}
inline CallResult LayoutCopy(Method method, void* target_behavior, void* source_plugin) {
    return native::CallVoid(method, target_behavior, source_plugin);
}
inline CallResult LayoutClear(Method method, void* source_behavior) {
    return native::CallVoid(method, source_behavior);
}
inline CallResult LayoutReference(Method method, void* field, void* plugin, void* value) {
    return native::CallVoid(method, field, plugin, value, 0, static_cast<void*>(nullptr), static_cast<void*>(nullptr));
}
}
