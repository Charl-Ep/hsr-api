#pragma once
#include <hksr/native/Invocation.hpp>

namespace hksr::api::detail {
inline CallResult StartBgmSync(Method call, void* component, void* retained_config) {
    return native::CallVoid(call, component, retained_config);
}
inline CallResult StopBgmSync(Method call, void* component) {
    return native::CallVoid(call, component);
}
}
