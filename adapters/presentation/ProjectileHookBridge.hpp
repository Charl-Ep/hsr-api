#pragma once
namespace hksr::api::detail {
template <auto Body> void* __fastcall ProjectileWrapperEntry(void* carrier, void* effect, void* invocation) {
    return Body(carrier, effect, invocation);
}
template <auto Body> void __fastcall ProjectileConstructionEntry(void* object, void* name, void* invocation) {
    Body(object, name, invocation);
}
}
