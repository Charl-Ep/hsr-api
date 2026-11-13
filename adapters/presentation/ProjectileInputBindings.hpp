#pragma once
#include <hksr/presentation/projectiles/ProjectileInputs.hpp>
#include <hksr/unity/ComponentConfig.hpp>
#include <set>
namespace hksr::api::detail {
struct ProjectileInputSchema {
    bool legacy = false;
    ProjectileFamily family = ProjectileFamily::Linear;
    std::vector<DeclaredField> numeric;
    DeclaredField hit_effect, fly_effect, fly_unique_name, align_hit, attach_hit, start_from_last, use_last_velocity;
    DeclaredField trigger_hit_callback, enable_raycast, correct_formation;
};
ProjectileInputSchema ResolveProjectileInputSchema(ObjectMetadata&, void* source);
std::set<std::string> ProjectileRequiredNumericInputs(ProjectileFamily);
DeclaredField ResolveProjectileExpressionField(ObjectMetadata&, void* dynamic);
Method ResolveProjectileInputClone(MethodResolver&);
Method ResolveProjectileDynamicFloat(MethodResolver&);
Method ResolveProjectileEffectNameLength(MethodResolver&);
ComponentConfigMethods ResolveProjectileResourceText(MethodResolver&);
}
