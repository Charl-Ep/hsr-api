#pragma once
#include "../../src/entity/PartnerEventCleanup.hpp"

namespace hksr::api::detail {
void ValidatePartnerCleanupBindings(const PartnerEventMethods&);
ObjectResult ResolvePartnerGroupType(ManagedObjects&);
ObjectResult AllocatePartnerEventCallback(ManagedObjects&);
PartnerEventMethods ResolvePartnerEventCleanup(RuntimeMetadata&);
}
