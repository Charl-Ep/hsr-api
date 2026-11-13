#pragma once
#include <hksr/Export.hpp>
#include <string>

namespace hksr::api {
enum class RuntimeAvailability {
    WaitingModules,
    WaitingTable,
    WaitingDomain,
    WaitingAssemblies,
    Available,
    Unsupported,
    WaitingInitialization
};
struct RuntimeAvailabilityResult {
    RuntimeAvailability state = RuntimeAvailability::WaitingModules;
    std::string error; // failure reason, or nonblocking image diagnostics when Available
};
// Cheap pre-bootstrap observation: no attachment, metadata inventory, hooks,
// module loading or obfuscated export invocation. Available means bootstrap
// inputs exist and the official initialization critical section has completed,
// not that a scene or every game declaration is ready. Never forces GC setup.
HKSR_API RuntimeAvailabilityResult ObserveRuntimeAvailability();
}
