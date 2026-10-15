#pragma once
#include <memory>
#include "WorldFrames.hpp"
#include "../entity/MapNpcInputs.hpp"

namespace hksr::sdk_v1 {
struct WorldPlacementMethods {
    Method player, owner_world, game_object, transform, position, rotation;
    std::vector<std::string> binding_errors;
};
struct PlayerPlacement {
    Position3 position;
    Rotation4 rotation;
    CallResult position_read,rotation_read;
    std::string error;
    bool ready=false;
};
// Borrowed local player only as a placement reference, never an asset source
// or a target for writes. Read on the owning official frame; retain no object.
class WorldPlacement {
public:
    HKSR_API explicit WorldPlacement(WorldPlacementMethods methods);
    HKSR_API ~WorldPlacement();
    HKSR_API WorldPlacement(const WorldPlacement&) noexcept;
    HKSR_API WorldPlacement& operator=(const WorldPlacement&) noexcept;
    HKSR_API PlayerPlacement ReadPlayer(const WorldFrame& frame) const;
    HKSR_API PlayerPlacement ReadEntity(GameEntity* entity) const;
    HKSR_API ObjectResult Root(GameEntity* entity) const; // borrowed current entity hierarchy root
    HKSR_API GameEntity* PlayerEntity(const WorldFrame& frame) const;
private: std::shared_ptr<const WorldPlacementMethods> methods_;
};
HKSR_API WorldPlacementMethods ResolveWorldPlacementMethods(MethodResolver& source);
}
