#pragma once
namespace hksr::api::detail {
template <class Body, int Index> struct MouseModeThunk {
    static void __fastcall Invoke(void* controller, void* metadata) {
        const auto own = Body::Before();
        reinterpret_cast<void(__fastcall*)(void*, void*)>(Body::Original(Index))(controller, metadata);
        Body::After(controller, own);
    }
    static void* Address() {
        return reinterpret_cast<void*>(&Invoke);
    }
};
}
