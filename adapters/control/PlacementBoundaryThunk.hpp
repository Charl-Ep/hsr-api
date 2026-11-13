#pragma once
namespace hksr::api::detail {
template <class Body> struct PlacementBoundaryThunk {
    static void __fastcall Invoke(void* controller, void* metadata) {
        reinterpret_cast<void(__fastcall*)(void*, void*)>(Body::Original())(controller, metadata);
        Body::After(controller);
    }
    static void* Address() {
        return reinterpret_cast<void*>(&Invoke);
    }
};
}
