#pragma once
#include <hksr/runtime/RuntimeAvailability.hpp>
#include <Windows.h>

namespace hksr::api::detail {
inline RuntimeAvailability ObserveInitialization(CRITICAL_SECTION* const* slot, const int* count) {
    __try {
        auto* mutex = *slot;
        if (!mutex)
            return RuntimeAvailability::WaitingInitialization;
        // TryEnter is recursive: observing on the initializer itself must not
        // mistake its in-progress count for a completed initialization.
        if (mutex->OwningThread == reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(GetCurrentThreadId())))
            return RuntimeAvailability::WaitingInitialization;
        if (!TryEnterCriticalSection(mutex))
            return RuntimeAvailability::WaitingInitialization;
        int initialized = 0;
        __try {
            initialized = *count;
        } __finally {
            LeaveCriticalSection(mutex);
        }
        return initialized > 0 ? RuntimeAvailability::Available : RuntimeAvailability::WaitingInitialization;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return RuntimeAvailability::Unsupported;
    }
}
}
