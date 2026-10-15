#include <hksr/native/ObjectAccess.hpp>
#include <Windows.h>

namespace hksr::sdk_v1::native {
namespace {
template<class Argument>
ObjectResult Invoke(void* entry, Argument value) {
    ObjectResult result;
    if (!entry || !value) return result;
    __try {
        result.object = reinterpret_cast<void*(__fastcall*)(Argument)>(entry)(value);
        result.call = {true, 0};
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result.call = {false, GetExceptionCode()};
    }
    return result;
}
}
ObjectResult NewString(ObjectFunctions functions, const char* utf8) {
    return Invoke(functions.string_new, utf8);
}
ObjectResult AllocateObject(ObjectFunctions functions, void* klass) {
    return Invoke(functions.object_new, klass);
}
ObjectResult TypeObject(ObjectFunctions functions, void* klass) {
    if (!functions.class_get_type || !functions.type_get_object) return {};
    const auto type = Invoke(functions.class_get_type, klass);
    if (!type.call.returned || !type.object) return type;
    return Invoke(functions.type_get_object, type.object);
}
}
