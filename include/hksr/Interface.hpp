#pragma once

// A new namespace is required for incompatible C++ interface changes.
// Game adaptation revisions keep this namespace when the contract is unchanged.
namespace hksr::sdk_v1 {
inline constexpr unsigned InterfaceGeneration = 1;
}
namespace hksr { namespace api = sdk_v1; }
