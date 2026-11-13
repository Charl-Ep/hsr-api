#pragma once
namespace hksr::api::detail {
inline void ForwardEffectVoid(void* entry, void* object, void* info) {
    reinterpret_cast<void(__fastcall*)(void*, void*)>(entry)(object, info);
}
inline void ForwardEffectTick(void* entry, void* object, float delta, void* info) {
    reinterpret_cast<void(__fastcall*)(void*, float, void*)>(entry)(object, delta, info);
}
inline void ForwardEffectVisibility(void* entry, void* object, bool visible, void* info) {
    reinterpret_cast<void(__fastcall*)(void*, bool, void*)>(entry)(object, visible, info);
}
inline void ForwardEffectSubmission(void* entry, void* manager, void* request, void* info) {
    reinterpret_cast<void(__fastcall*)(void*, void*, void*)>(entry)(manager, request, info);
}
inline void* ForwardEffectCreation(void* entry, void* manager, void* request, void* info) {
    return reinterpret_cast<void*(__fastcall*)(void*, void*, void*)>(entry)(manager, request, info);
}
inline void* ForwardEffectQuery(void* entry, void* manager, void* name, void* target, void* info) {
    return reinterpret_cast<void*(__fastcall*)(void*, void*, void*, void*)>(entry)(manager, name, target, info);
}
}
