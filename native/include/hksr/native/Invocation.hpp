#pragma once
#include <Windows.h>
#include <hksr/metadata/MethodResolver.hpp>

namespace hksr::sdk_v1::native {
// In-process adapter/research primitive. Caller supplies the exact qualified
// signature and a valid attached thread/object lifetime. SEH is a receipt, not
// validation of an arbitrary function address or permission to retry a write.
template<class Result,class... Args>
CallResult Call(Method method,Result* result,Args... args){
    if(!method.code)return {};
    __try {
        *result=reinterpret_cast<Result(__fastcall*)(Args...,void*)>(method.code)(args...,method.metadata);
        return {true,0};
    }__except(EXCEPTION_EXECUTE_HANDLER){return {false,GetExceptionCode()};}
}
template<class... Args>
CallResult CallVoid(Method method,Args... args){
    if(!method.code)return {};
    __try {
        reinterpret_cast<void(__fastcall*)(Args...,void*)>(method.code)(args...,method.metadata);
        return {true,0};
    }__except(EXCEPTION_EXECUTE_HANDLER){return {false,GetExceptionCode()};}
}
}
