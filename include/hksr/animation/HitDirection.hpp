#pragma once
#include "../entity/MapNpcInputs.hpp"

namespace hksr::sdk_v1 {
struct HitDirectionMethods {
    Method inverse_direction,rotation,look_rotation,to_euler;
    std::vector<std::string> binding_errors;
};
// Read-only spatial part of the official hit-angle calculation. Input is a
// world-space direction, not a point. The caller owns the current Transform.
// Does not set Animator parameters, update gameplay dictionaries or dispatch hits.
class HKSR_API HitDirection {
public:
    explicit HitDirection(HitDirectionMethods);
    Observed<float> Read(void* transform,Position3 world_direction,bool mirrored=false) const;
private:
    HitDirectionMethods methods_;
};
HKSR_API HitDirectionMethods ResolveHitDirectionMethods(MethodResolver&);
}
