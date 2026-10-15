#pragma once
#include <hksr/managed/References.hpp>

namespace hksr::sdk_v1::native {
struct ObjectFunctions {
    void* string_new = nullptr;
    void* object_new = nullptr;
    void* class_get_type = nullptr;
    void* type_get_object = nullptr;
};
// Native IL2CPP API entries, not managed methods: no trailing MethodInfo.
ObjectResult NewString(ObjectFunctions functions, const char* utf8);
ObjectResult AllocateObject(ObjectFunctions functions, void* klass);
ObjectResult TypeObject(ObjectFunctions functions, void* klass);
}
