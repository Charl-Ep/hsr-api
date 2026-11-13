#pragma once
#include <hksr/presentation/effects/EffectEntityFollow.hpp>
#include <hksr/presentation/effects/HitEffectControl.hpp>
#include <hksr/presentation/movement/MovementGeometry.hpp>
#include <hksr/presentation/events/AnimationEventPolicy.hpp>

namespace hksr::api::detail {
enum class ActorComponent { Model, Visibility, Transform, FreeStyle, CharacterData };
ObjectResult ResolveActorComponentType(ManagedObjects&, ActorComponent);
std::vector<DeclaredField> ReadMonoEffectFields(RuntimeMetadata&);
std::vector<DeclaredField> ReadTriggerEffectFields(RuntimeMetadata&);
HitEffectControlMethods ResolveHitEffects(RuntimeMetadata&);
MovementGeometryAccess ResolveActorMovementGeometry(RuntimeMetadata&);
std::size_t ResolveStoryEventOffset(RuntimeMetadata&);
struct EffectFollowBindings {
    EffectEntityFollowMethods methods;
    EffectEntityFollowFields fields;
    ReferenceListMethods list;
    EffectMotionTargetBinding motion;
};
EffectFollowBindings ResolveEffectFollowBindings(RuntimeMetadata&);
SelfPerformerRule HeadLookPerformerRule();
}
