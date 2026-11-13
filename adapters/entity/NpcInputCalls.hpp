#pragma once
#include <hksr/entity/MapNpcInputs.hpp>
#include <hksr/native/Invocation.hpp>
namespace hksr::api::detail {
inline CallResult CallNpcDefinitionConstructor(Method method, void* definition, std::uint32_t id, void* label,
                                               Position3 position, Rotation4 rotation, void* level) {
    return native::CallVoid(method, definition, static_cast<void*>(nullptr), std::uint32_t{0}, std::uint32_t{0},
                            std::uint32_t{0}, id, label, &position, &rotation, level, static_cast<void*>(nullptr));
}
}
