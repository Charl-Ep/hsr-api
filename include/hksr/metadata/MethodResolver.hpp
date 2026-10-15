#pragma once
#include "../Export.hpp"
#include <initializer_list>

namespace hksr::sdk_v1 {
// Adapter-side descriptor. Application code must not manufacture game pointers.
struct Method { void* code=nullptr; void* metadata=nullptr; };
enum class MethodKind { Instance, Static };
struct HKSR_API MethodResolver {
    virtual ~MethodResolver()=default;
    virtual Method Resolve(const char* name_space,const char* type,const char* name,
        const char* result,std::initializer_list<const char*> parameters,MethodKind kind=MethodKind::Instance)=0;
};
struct CallResult {
    bool returned=false;
    unsigned long exception=0;
};
template<class T> struct Observed { T value{}; CallResult read{}; };
}
