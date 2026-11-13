#pragma once
#include <Windows.h>

namespace hksr::api::detail {
template <class Body> struct ActorEmotionThunk {
    static void __fastcall Invoke(void* component, void* metadata) {
        const auto error = GetLastError();
        const bool handled = Body::Handle(component);
        SetLastError(error);
        if (!handled)
            reinterpret_cast<void(__fastcall*)(void*, void*)>(Body::Original())(component, metadata);
    }
    static void* Address() {
        return reinterpret_cast<void*>(&Invoke);
    }
};
}
