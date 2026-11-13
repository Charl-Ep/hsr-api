#include "MeshWriterCalls.hpp"
#include <hksr/native/Invocation.hpp>

namespace hksr::api::detail {
static_assert(sizeof(FixedBoneWeights) == 32 && sizeof(BindposeMatrix) == 64);
Observed<bool> WriteWeights(void* entry, void* mesh, const FixedBoneWeights* data, std::int32_t count) {
    Observed<bool> out;
    if (!entry)
        return out;
    __try {
        out.value = reinterpret_cast<bool(__fastcall*)(void*, const void*, std::int32_t)>(entry)(mesh, data, count);
        out.read = {true, 0};
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out.read = {false, GetExceptionCode()};
    }
    return out;
}
CallResult WriteBindposes(void* entry, void* mesh, const BindposeMatrix* data, std::int32_t count) {
    if (!entry)
        return {};
    __try {
        reinterpret_cast<void(__fastcall*)(void*, const void*, std::int32_t)>(entry)(mesh, data, count);
        return {true, 0};
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return {false, GetExceptionCode()};
    }
}
}
