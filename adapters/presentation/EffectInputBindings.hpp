#pragma once
#include <hksr/presentation/effects/EffectInputs.hpp>
#include <map>
namespace hksr::api::detail {
enum class EffectInputField {
    Manager,
    Caster,
    Target,
    ParamEntity,
    Completion,
    Flags,
    UniqueName,
    AttachPoint,
    PositionOffset,
    RotationOffset,
    Scale,
    FadeIn,
    SimulateImmediately,
    Replay,
    ParamUsage,
    MaxPitch,
    InitialPosition,
    AnchorPosition,
    InitialRotation,
    IgnorePrefabAttachment
};
struct EffectInputFieldIdentity {
    const char* owner;
    const char* name;
    bool is_static = false;
};
EffectInputFieldIdentity DescribeEffectInputField(EffectInputField);
using EffectInputFields = std::map<EffectInputField, DeclaredField>;
EffectInputFields ResolveEffectInputFields(ObjectMetadata&, void*, std::initializer_list<EffectInputField>);
ObjectResult ResolveEffectInputOwnerType(ManagedObjects&, EffectInputField);
ObjectResult ResolveEffectFlagType(ManagedObjects&);
Method ResolveEffectAttachmentSetter(MethodResolver&);
Method ResolveEffectLookRotation(MethodResolver&);
struct EffectNullableVector {
    Position3 value;
    std::uint8_t present;
    std::uint8_t padding[3]{};
};
struct EffectNullableRotation {
    Rotation4 value;
    std::uint8_t present;
    std::uint8_t padding[3]{};
};
static_assert(sizeof(EffectNullableVector) == 16 && offsetof(EffectNullableVector, present) == 12);
static_assert(sizeof(EffectNullableRotation) == 20 && offsetof(EffectNullableRotation, present) == 16);
struct EffectInputConventions {
    static constexpr int resident_flag = 1, impact_flag = 6, look_at_usage = 1, impact_usage = 5;
    static constexpr float toward_max_pitch = 90.f;
};
}
