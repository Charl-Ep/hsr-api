#pragma once
#include <hksr/control/PartnerControlSession.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <cstddef>
#include <hksr/native/Invocation.hpp>
namespace hksr::api::detail {
enum class PartnerComponent { Group, Transform, AnimationFollow, Visibility, BattleAnimation };
ObjectResult ResolvePartnerComponentType(ManagedObjects&, PartnerComponent);
PartnerControlMethods ResolvePartnerControlBindings(RuntimeMetadata&);
inline CallResult CallPartnerIdle(Method method, void* component, void* name, const PartnerIdleAnimation& input) {
    return native::CallVoid(method, component, std::int32_t{1}, name, input.normalized_start,
                            input.normalized_transition, input.normalized_end, std::int32_t{0}, std::uint32_t{0},
                            false);
}
struct AttachValue {
    void* self_point = nullptr;
    GameEntity* target = nullptr;
    void* target_point = nullptr;
    bool disable_when_invisible = false, use_target_scale = false;
    std::uint16_t padding = 0;
    std::int32_t option = 0;
    VisibilityFollow visibility = VisibilityFollow::None;
    std::uint16_t padding2 = 0;
    std::uint32_t padding3 = 0;
    GameEntity* extra_target = nullptr;
};
static_assert(sizeof(AttachValue) == 48 && offsetof(AttachValue, visibility) == 32 &&
              offsetof(AttachValue, extra_target) == 40);
struct VisibilityValue {
    void* key = nullptr;
    bool collider = false, trigger = false, audio = false;
    unsigned char padding[5]{};
};
static_assert(sizeof(VisibilityValue) == 16 && offsetof(VisibilityValue, collider) == 8 &&
              offsetof(VisibilityValue, audio) == 10);
}
