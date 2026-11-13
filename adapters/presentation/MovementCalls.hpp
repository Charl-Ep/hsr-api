#pragma once
#include <hksr/entity/MapNpcInputs.hpp>
namespace hksr::api::detail {
template <auto Update> bool __fastcall MovementUpdateBridge(void* self, float delta, Position3* position, void* info) {
    return Update(self, delta, position, info);
}
inline bool ForwardAnimationMovementUpdate(void* entry, void* self, float delta, Position3* position, void* info) {
    return reinterpret_cast<bool(__fastcall*)(void*, float, Position3*, void*)>(entry)(self, delta, position, info);
}
}
