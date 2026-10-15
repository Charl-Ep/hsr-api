#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>

namespace hksr::sdk_v1::detail {
// Only the shared process RuntimeHost's provider can acquire this lease.
std::shared_ptr<RuntimeMetadata> RetainRuntimeMetadata(RuntimeMetadata&);
// Existing engine frame delivery calls this even after the client stopped.
void ServiceRetiredPresentations() noexcept;
}
