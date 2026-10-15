#include <hksr/runtime/RuntimeHost.hpp>

// Same source-level function and result, but a different interface namespace.
namespace hksr::sdk_incompatible_test {
__declspec(dllexport) hksr::api::RuntimeHostStatus ReadRuntimeHostStatus() {return {};}
}
