#pragma once
#include "MetadataReader.hpp"

namespace hksr::sdk_v1::native {
// Adapter-owned, already qualified native table and layout. This entry does
// not discover/validate a game build or enable actor/runtime services. The
// table, domain and callback dependencies must outlive the returned provider.
class MetadataProvider {
public:
    HKSR_API static std::unique_ptr<RuntimeMetadata> Open(RuntimeVersion version,MetadataConnection connection);
};
}
