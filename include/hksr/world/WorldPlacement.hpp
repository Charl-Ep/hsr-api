#pragma once
#include <memory>
#include "WorldFrames.hpp"
#include <hksr/math/SpatialTypes.hpp>
#include "../managed/References.hpp"

namespace hksr::api {
struct GameEntity;
struct WorldPlacementMethods {
    Method player, owner_world, game_object, transform, position, rotation;
    std::vector<std::string> binding_errors;
};
struct PlacementSnapshot {
    Position3 position;
    Rotation4 rotation;
    CallResult position_read, rotation_read;
    std::string error;
    bool ready = false;
};
// Read-only entity transforms. The local player is a placement reference, not
// an asset source. Read on the owning official frame; retain no object.
class WorldPlacement {
  public:
    HKSR_API explicit WorldPlacement(WorldPlacementMethods methods);
    HKSR_API ~WorldPlacement();
    HKSR_API WorldPlacement(const WorldPlacement&) noexcept;
    HKSR_API WorldPlacement& operator=(const WorldPlacement&) noexcept;
    HKSR_API PlacementSnapshot ReadPlayer(const WorldFrame& frame) const;
    HKSR_API PlacementSnapshot ReadEntity(GameEntity* entity) const;
    HKSR_API ObjectResult Root(GameEntity* entity) const; // borrowed current entity hierarchy root
    HKSR_API GameEntity* PlayerEntity(const WorldFrame& frame) const;

  private:
    std::shared_ptr<const WorldPlacementMethods> methods_;
};
HKSR_API WorldPlacementMethods ResolveWorldPlacementMethods(MethodResolver& source);
}
