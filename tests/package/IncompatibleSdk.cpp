#include <hksr/runtime/RuntimeHost.hpp>

// The expected zero-argument export is absent from this incompatible fixture.
namespace hksr::api {
__declspec(dllexport) RuntimeHostStatus ReadRuntimeHostStatus(int) {
    return {};
}
}
