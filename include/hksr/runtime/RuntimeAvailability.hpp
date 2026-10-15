#pragma once
#include "GameVersion.hpp"
#include <hksr/Export.hpp>
#include <string>

namespace hksr::sdk_v1 {
enum class RuntimeAvailability { WaitingModules, WaitingTable, WaitingDomain, WaitingAssemblies, Available, Unsupported, WaitingInitialization };
struct RuntimeAvailabilityResult {
    RuntimeAvailability state=RuntimeAvailability::WaitingModules;
    std::string error;
};
// Cheap pre-bootstrap observation: no attachment, metadata inventory, hooks,
// module loading or obfuscated export invocation. Available means bootstrap
// inputs exist and the official initialization critical section has completed,
// not that a scene or every game declaration is ready. Never forces GC setup.
HKSR_API RuntimeAvailabilityResult ObserveRuntimeAvailability(RuntimeVersion version);
HKSR_API RuntimeAvailabilityResult ObserveRuntimeAvailability();
}
