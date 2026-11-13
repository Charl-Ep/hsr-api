#pragma once
#include <hksr/presentation/projectiles/OwnedProjectileGroup.hpp>
#include <array>
#include <utility>
#include <hksr/native/Invocation.hpp>
namespace hksr::api::detail {
enum class ProjectileObject { Component, Configuration, Ability, SkillContext, Carrier };
struct ProjectileGroupBindings {
    Method component_ctor, owner_ref, config_ctor, ability_ctor, ability_dispose, context_get, skill_ctor, list_get,
        tick;
    Method carrier_ctor, carrier_dispose, partial_cleanup, previous_set;
    std::size_t wrapper = 0, position = 0, velocity = 0, hit_position = 0, direction = 0, phase = 0, ordered = 0,
                last_position = 0, last_velocity = 0, owner_kind = 0;
    std::array<std::size_t, 4> ability_tasks{};
};
ProjectileGroupBindings ResolveProjectileGroupBindings(const ProjectileGroupServices&);
ObjectResult AllocateProjectileObject(ManagedObjects&, ProjectileObject);
void SetProjectileSkillContext(const ProjectileGroupServices&, void* receiver, void* context,
                               const std::function<void*(ObjectResult)>& retain);
std::pair<Method, Method> ResolveProjectileListMethods(ObjectMetadata&, void* list);
bool IsProjectileOwnerKind(int kind) noexcept;
bool ProjectileHasArrived(int state) noexcept;
inline CallResult CallProjectileCarrierConstructor(Method method, void* carrier, const ProjectileInputStatus& input,
                                                   void* ability, GameEntity* target, GameEntity* owner, void* skill,
                                                   bool ordered) {
    return native::CallVoid(method, carrier, input.legacy ? input.parameters : nullptr, ability, target, owner,
                            static_cast<void*>(nullptr), skill, static_cast<void*>(nullptr), ordered,
                            input.legacy ? nullptr : input.parameters);
}
}
