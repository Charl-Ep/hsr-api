#pragma once
#include <memory>
#include "WorldFrames.hpp"
#include <array>

namespace hksr::sdk_v1 {
// Frame-local unrooted references. Consume on the official world thread;
// retain identities before keeping them across callbacks. A readable map is
// NOT proof that loading, gameplay or rendering is ready.
struct WorldMapSnapshot {
    Observed<void*> group_manager, map;
    // Plane, Floor, Dimension, Content, StoryLine, GameMode (enum bits).
    std::array<Observed<std::uint32_t>,6> identity;
    Observed<std::uint64_t> runtime_id; // observation only, not a generation
    Observed<bool> pending_switch;
};
struct WorldMapMethods {
    Method group_manager, map, pending_switch, runtime_id;
    std::array<Method,6> identity;
    std::vector<std::string> binding_errors;
};
class WorldMap {
public:
    HKSR_API explicit WorldMap(WorldMapMethods methods);
    HKSR_API ~WorldMap();
    HKSR_API WorldMap(const WorldMap&) noexcept;
    HKSR_API WorldMap& operator=(const WorldMap&) noexcept;
    HKSR_API WorldMapSnapshot Read(const WorldFrame& frame) const;
private:
    std::shared_ptr<const WorldMapMethods> methods_;
};
HKSR_API WorldMapMethods ResolveWorldMapMethods(MethodResolver& source);
}
