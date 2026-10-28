#pragma once
#include "../../metadata/RuntimeMetadata.hpp"
#include "../../world/WorldPlacement.hpp"

namespace hksr::sdk_v1 {
struct MovementGeometry {float radius=0;Position3 local_offset;};
// CharacterData values consumed by the official movement-target producer.
// Collision bounds and renderer bounds have different semantics.
struct MovementGeometryAccess {
    Method radius;
    std::size_t local_offset=0;
    HKSR_API Observed<MovementGeometry> Read(void* component) const;
};
HKSR_API MovementGeometryAccess ResolveMovementGeometryAccess(MethodResolver&,const std::vector<DeclaredField>&);
}
