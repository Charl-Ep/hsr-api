#pragma once
namespace hksr::api::detail {
template <auto Body> void __fastcall EffectObjectEntry(void* object, void* invocation) {
    Body(object, invocation);
}
template <auto Body> void __fastcall EffectTickEntry(void* object, float delta, void* invocation) {
    Body(object, delta, invocation);
}
template <auto Body> void __fastcall EffectVisibilityEntry(void* object, bool requested, void* invocation) {
    Body(object, requested, invocation);
}
template <auto Body> void __fastcall EffectSubmissionEntry(void* manager, void* request, void* invocation) {
    Body(manager, request, invocation);
}
template <auto Body> void* __fastcall EffectCreationEntry(void* manager, void* request, void* invocation) {
    return Body(manager, request, invocation);
}
template <auto Body> void* __fastcall EffectQueryEntry(void* manager, void* name, void* target, void* invocation) {
    return Body(manager, name, target, invocation);
}
}
