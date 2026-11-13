#pragma once
#include "../../src/model/ActorInputPolicy.hpp"
#include <Windows.h>

namespace hksr::api::detail {
// Native argument order and MethodInfo belong to the current game adapter.
// Body selects an input using the domain owner/thread policy.
template <class Body, ActorInputKind Kind> struct ActorInputThunk {
    static void __fastcall Invoke(void* receiver, void* input, void* metadata) {
        const auto error = GetLastError();
        auto* selected = Body::template Select<Kind>(receiver, input);
        SetLastError(error);
        reinterpret_cast<void(__fastcall*)(void*, void*, void*)>(Body::template Original<Kind>())(receiver, selected,
                                                                                                  metadata);
    }
    static void* Address() {
        return reinterpret_cast<void*>(&Invoke);
    }
};
}
