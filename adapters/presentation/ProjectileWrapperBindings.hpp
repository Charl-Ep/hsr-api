#pragma once
#include <hksr/metadata/MethodResolver.hpp>
namespace hksr::api::detail {
struct ProjectileWrapperBindings {
    Method factory;
    void* constructor = nullptr;
    Method active;
};
ProjectileWrapperBindings ResolveProjectileWrapperBindings(MethodResolver&);
inline void* ForwardProjectileWrapperFactory(void* entry, void* carrier, void* effect, void* info) {
    return reinterpret_cast<void*(__fastcall*)(void*, void*, void*)>(entry)(carrier, effect, info);
}
inline void ForwardProjectileWrapperConstruction(void* entry, void* object, void* name, void* info) {
    reinterpret_cast<void(__fastcall*)(void*, void*, void*)>(entry)(object, name, info);
}
}
