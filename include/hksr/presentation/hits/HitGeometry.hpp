#pragma once
#include "../../entity/MapNpcInputs.hpp"
#include "../effects/HitEffectControl.hpp"
#include <optional>

namespace hksr::sdk_v1 {
// CharacterCollisionType, not the differently numbered HitBox union tag.
enum class HitShapeKind { Capsule=0, Box=1 };
struct HitShapeParameters {
    HitShapeKind kind=HitShapeKind::Capsule;
    float width=0,height=0; // length is not consumed by hit-direction/effect placement
    Position3 offset;
};
struct HitShapeSnapshot {
    Position3 center;
    float vertical_extent=0,effect_radius=0;
    bool attached=false;
};
struct HitGeometryMethods {
    Method transform_vector,position,direction,inverse_point;
    std::size_t logical_position_offset=0;
    bool logical_position_available=false;
    std::vector<std::string> binding_errors;
    Method attach_point;
    Method world_scale;
};
// Read-only projection of the official hit-shape inputs. Does not allocate or
// write AbilityComponent, register collisions, or choose a configuration source.
class HKSR_API HitGeometry {
public:
    explicit HitGeometry(HitGeometryMethods);
    Observed<Position3> LogicalPosition(void* transform_component) const;
    ObjectResult Anchor(void* model_component,void* retained_name) const;
    // anchor is the result of a successful named attachment lookup, or null
    // when no attachment exists. A failed lookup is not the null case.
    Observed<HitShapeSnapshot> Read(void* transform_component,void* anchor,const HitShapeParameters&) const;
    // Local effect offset before per-effect overrides and scale adjustment.
    // Negative height selects the official default local offset (zero).
    Observed<Position3> EffectOffset(void* receiver_transform,const HitShapeSnapshot&,
        Position3 attacker,float height,float angle_degrees) const;
    Observed<HitEffectPlacement> EffectPlacement(void* logical_transform,void* receiver_transform,const HitShapeSnapshot&,
        Position3 attacker,float height,float angle_degrees,const std::optional<Position3>& offset_override,
        bool offset_by_scale,float delay) const;
private:
    HitGeometryMethods methods_;
};
HKSR_API HitGeometryMethods ResolveHitGeometryMethods(MethodResolver&,const std::vector<DeclaredField>& transform_fields);
}
