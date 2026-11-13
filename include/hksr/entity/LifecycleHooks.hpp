#pragma once
#include "EntityLifecycle.hpp"

namespace hksr::api {
// One product monitor per SDK DLL, any number of tracked entities. Install and
// remove only at host-controlled quiescent points; never in DllMain. This does
// not install frame scheduling, model-pool restoration or diagnostic probes.
HKSR_API void InstallEntityLifecycleHooks(MethodResolver& resolver);
HKSR_API void RemoveEntityLifecycleHooksAtQuiescentPoint();
HKSR_API EntityLifecycle& TrackedEntities();
}
