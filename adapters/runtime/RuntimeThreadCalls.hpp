#pragma once
#include <hksr/runtime/RuntimeThread.hpp>
#include <Windows.h>

namespace hksr::api::detail {
inline ObjectResult RuntimeNoArgs(void* function) {
    ObjectResult result;
    if (!function)
        return result;
    __try {
        result.object = reinterpret_cast<void*(__fastcall*)()>(function)();
        result.call.returned = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result.call.exception = GetExceptionCode();
    }
    return result;
}
inline ObjectResult RuntimeAttach(void* function, void* domain) {
    ObjectResult result;
    __try {
        result.object = reinterpret_cast<void*(__fastcall*)(void*)>(function)(domain);
        result.call.returned = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result.call.exception = GetExceptionCode();
    }
    return result;
}
inline CallResult RuntimeDetach(void* function, void* thread) {
    CallResult result;
    __try {
        reinterpret_cast<void(__fastcall*)(void*)>(function)(thread);
        result.returned = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result.exception = GetExceptionCode();
    }
    return result;
}
}
