#pragma once
#include <hksr/presentation/effects/HitEffectControl.hpp>
#include <hksr/native/MetadataReader.hpp>
namespace hksr::api::detail {
inline bool WriteHitEffectPlacement(void* parameters, const HitEffectControlMethods& m, const HitEffectPlacement& p) {
    struct NullableVector {
        Position3 value;
        std::uint8_t present = 1;
        std::uint8_t padding[3]{};
    };
    static_assert(sizeof(NullableVector) == 16);
    const NullableVector value{p.offset};
    const auto ignore = static_cast<std::uint8_t>(p.ignore_prefab_attach_point);
    return parameters &&
           native::CopyMemoryChecked(static_cast<char*>(parameters) + m.position_offset, &value, sizeof(value)) &&
           native::CopyMemoryChecked(static_cast<char*>(parameters) + m.ignore_attach_offset, &ignore, sizeof(ignore));
}
}
