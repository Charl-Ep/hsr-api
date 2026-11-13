#include "../../../adapters/presentation/GeometryCalls.hpp"
#include <hksr/presentation/hits/HitGeometry.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <algorithm>
#include <cmath>
#include <utility>

namespace hksr::api {
namespace {
bool Finite(Position3 p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
bool Nonnegative(float x) {
    return std::isfinite(x) && x >= 0;
}
}
HitGeometry::HitGeometry(HitGeometryMethods methods) : methods_(std::move(methods)) {}
ObjectResult HitGeometry::Anchor(void* model, void* name) const {
    ObjectResult out;
    if (model && name)
        out.call = native::Call(methods_.attach_point, &out.object, model, name, false);
    return out;
}
Observed<Position3> HitGeometry::LogicalPosition(void* transform) const {
    Observed<Position3> out;
    if (!transform || !methods_.logical_position_available)
        return out;
    out.read.returned = native::CopyMemoryChecked(
                            &out.value, static_cast<const unsigned char*>(transform) + methods_.logical_position_offset,
                            sizeof(out.value)) &&
                        Finite(out.value);
    return out;
}
Observed<HitShapeSnapshot> HitGeometry::Read(void* transform, void* anchor,
                                             const HitShapeParameters& parameters) const {
    Observed<HitShapeSnapshot> out;
    if ((parameters.kind != HitShapeKind::Capsule && parameters.kind != HitShapeKind::Box) ||
        !Nonnegative(parameters.width) || !Nonnegative(parameters.height) || !Finite(parameters.offset))
        return out;
    Position3 position, offset;
    if (anchor) {
        out.read = detail::HitUnityPosition(methods_.position, anchor, &position);
        if (!out.read.returned || !Finite(position)) {
            out.read.returned = false;
            return out;
        }
        auto input = parameters.offset;
        out.read = detail::HitUnityDirection(methods_.direction, anchor, input, &offset);
    } else {
        const auto origin = LogicalPosition(transform);
        out.read = origin.read;
        if (!out.read.returned)
            return out;
        position = origin.value;
        position.y += parameters.height * .5f;
        out.read = detail::HitLogicalVector(methods_.transform_vector, transform, parameters.offset, &offset);
    }
    if (!out.read.returned || !Finite(offset)) {
        out.read.returned = false;
        return out;
    }
    auto& shape = out.value;
    shape.attached = anchor != nullptr;
    shape.center = {position.x + offset.x, position.y + offset.y, position.z + offset.z};
    shape.vertical_extent = parameters.height * .5f;
    if (parameters.kind == HitShapeKind::Capsule) {
        shape.vertical_extent += parameters.width;
        shape.effect_radius = parameters.width;
    } else
        shape.effect_radius = parameters.width * .5f;
    out.read.returned = Finite(shape.center) && std::isfinite(shape.vertical_extent);
    return out;
}
Observed<Position3> HitGeometry::EffectOffset(void* transform, const HitShapeSnapshot& shape, Position3 attacker,
                                              float height, float angle) const {
    Observed<Position3> out;
    if (!std::isfinite(height))
        return out;
    if (height < 0) {
        out.read.returned = true;
        return out;
    }
    if (!transform || !Finite(attacker) || !Finite(shape.center) || !Nonnegative(shape.vertical_extent) ||
        !Nonnegative(shape.effect_radius) || !std::isfinite(angle))
        return out;
    float dx = attacker.x - shape.center.x, dz = attacker.z - shape.center.z;
    const auto magnitude = std::sqrt(dx * dx + dz * dz);
    if (!std::isfinite(magnitude))
        return out;
    if (magnitude > 1.e-5f) {
        dx /= magnitude;
        dz /= magnitude;
    } else
        dx = dz = 0;
    // Source: atan2(deltaX,deltaZ), x=sin(angle), z=cos(angle).
    const float radians =
        static_cast<float>(std::atan2(static_cast<double>(dx), static_cast<double>(dz))) + angle * .01745329238474369f;
    const auto lower = shape.center.y - shape.vertical_extent, upper = shape.center.y + shape.vertical_extent;
    const auto desired = attacker.y + height;
    if (!std::isfinite(radians) || !std::isfinite(lower) || !std::isfinite(upper) || !std::isfinite(desired))
        return out;
    Position3 point{shape.center.x + static_cast<float>(std::sin(static_cast<double>(radians))) * shape.effect_radius,
                    std::clamp(desired, lower, upper),
                    shape.center.z + static_cast<float>(std::cos(static_cast<double>(radians))) * shape.effect_radius};
    if (!Finite(point))
        return out;
    out.read = detail::HitUnityInversePoint(methods_.inverse_point, transform, point, &out.value);
    if (!Finite(out.value))
        out.read.returned = false;
    return out;
}
Observed<HitEffectPlacement> HitGeometry::EffectPlacement(void* logical, void* root, const HitShapeSnapshot& shape,
                                                          Position3 attacker, float height, float angle,
                                                          const std::optional<Position3>& override_offset, bool scaled,
                                                          float delay) const {
    Observed<HitEffectPlacement> out;
    if (!Finite(attacker) || !Finite(shape.center) || !Nonnegative(shape.vertical_extent) || !std::isfinite(height) ||
        !std::isfinite(delay))
        return out;
    auto& p = out.value;
    p.direction = {attacker.x - shape.center.x, attacker.y - std::max(0.f, shape.center.y - shape.vertical_extent),
                   attacker.z - shape.center.z};
    if (override_offset) {
        if (!Finite(*override_offset))
            return out;
        p.offset = *override_offset;
    } else {
        const auto offset = EffectOffset(root, shape, attacker, height, angle);
        out.read = offset.read;
        if (!out.read.returned)
            return out;
        p.offset = offset.value;
    }
    if (scaled && logical) {
        Position3 scale;
        out.read = detail::HitWorldScale(methods_.world_scale, logical, &scale);
        if (!out.read.returned || !Finite(scale)) {
            out.read.returned = false;
            return out;
        }
        p.offset = {p.offset.x * scale.x, p.offset.y * scale.y, p.offset.z * scale.z};
    } // Native absent TransformComponent branch uses unit scale.
    p.delay = delay;
    p.ignore_prefab_attach_point = height >= 0;
    out.read.returned = Finite(p.direction) && Finite(p.offset);
    return out;
}

}
