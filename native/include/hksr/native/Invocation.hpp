#pragma once
#include <Windows.h>
#include <hksr/metadata/MethodResolver.hpp>
#if defined(HKSR_API_BUILD)
#include <hksr/runtime/OperationTrace.hpp>
#endif

namespace hksr::sdk_v1::native {
// In-process adapter/research primitive. Caller supplies the exact qualified
// signature and a valid attached thread/object lifetime. SEH is a receipt, not
// validation of an arbitrary function address or permission to retry a write.
template<class Result,class... Args>
static CallResult RawCall(Method method,Result* result,Args... args){
    if(!method.code)return {};
    __try {
        *result=reinterpret_cast<Result(__fastcall*)(Args...,void*)>(method.code)(args...,method.metadata);
        return {true,0};
    }__except(EXCEPTION_EXECUTE_HANDLER){return {false,GetExceptionCode()};}
}
template<class... Args>
static CallResult RawCallVoid(Method method,Args... args){
    if(!method.code)return {};
    __try {
        reinterpret_cast<void(__fastcall*)(Args...,void*)>(method.code)(args...,method.metadata);
        return {true,0};
    }__except(EXCEPTION_EXECUTE_HANDLER){return {false,GetExceptionCode()};}
}
// Internal linkage keeps SDK instrumentation separate from standalone
// NativeSupport clients, which must remain usable without the runtime DLL.
template<class Result,class... Args>
static CallResult Call(Method method,Result* result,Args... args) {
#if defined(HKSR_API_BUILD)
    OperationSpan span("native.call",reinterpret_cast<std::uintptr_t>(method.code));
#endif
    const auto out=RawCall(method,result,args...);
#if defined(HKSR_API_BUILD)
    if(!out.returned)span.Failed();
#endif
    return out;
}
template<class... Args>
static CallResult CallVoid(Method method,Args... args) {
#if defined(HKSR_API_BUILD)
    OperationSpan span("native.call",reinterpret_cast<std::uintptr_t>(method.code));
#endif
    const auto out=RawCallVoid(method,args...);
#if defined(HKSR_API_BUILD)
    if(!out.returned)span.Failed();
#endif
    return out;
}
}
