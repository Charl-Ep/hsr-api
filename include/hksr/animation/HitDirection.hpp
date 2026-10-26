#pragma once
#include "../entity/MapNpcInputs.hpp"

namespace hksr::sdk_v1 {
struct HitDirectionMethods {
    Method inverse_direction,rotation,look_rotation,to_euler;
    std::vector<std::string> binding_errors;
    Method calculate_world;
    std::size_t mirror_offset=0;
    bool mirror_available=false;
};
// Read-only spatial part of the official hit-angle calculation. Input is a
// world-space direction, not a point. The caller owns the current Transform.
// Does not set Animator parameters, update gameplay dictionaries or dispatch hits.
class HKSR_API HitDirection {
public:
    explicit HitDirection(HitDirectionMethods);
    Observed<float> Read(void* transform,Position3 world_direction,bool mirrored=false) const;
    // Horizontal offset already includes any selected random contribution.
    // Uses the attacker's logical position and receiver's current hit center.
    Observed<Position3> WorldDirection(Position3 attacker,Position3 center,
        float horizontal_degrees=0,float vertical_degrees=0) const;
    Observed<bool> Mirrored(void* model_component) const;
private:
    HitDirectionMethods methods_;
};
HKSR_API HitDirectionMethods ResolveHitDirectionMethods(MethodResolver&);
HKSR_API void ResolveHitDirectionModel(HitDirectionMethods&,const std::vector<DeclaredField>&);
}
