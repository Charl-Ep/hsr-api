#pragma once
#include <hksr/presentation/projectiles/ProjectilePhysics.hpp>
namespace hksr::api::detail {
enum class ProjectilePhysicsComponent { Rigidbody, Collider };
ObjectResult ResolveProjectilePhysicsType(ManagedObjects&, ProjectilePhysicsComponent);
}
