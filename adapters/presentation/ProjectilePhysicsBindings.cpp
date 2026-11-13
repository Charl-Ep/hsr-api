#include "ProjectilePhysicsBindings.hpp"
namespace hksr::api {
ProjectilePhysicsMethods ResolveProjectilePhysics(MethodResolver& m) {
    ProjectilePhysicsMethods out;
    out.components = m.Resolve("UnityEngine", "GameObject", "GetComponentsInChildren", "UnityEngine.Component[]",
                               {"System.Type", "System.Boolean"});
    out.collider_get = m.Resolve("UnityEngine", "Collider", "get_enabled", "System.Boolean", {});
    out.collider_set = m.Resolve("UnityEngine", "Collider", "set_enabled", "System.Void", {"System.Boolean"});
    out.rigidbody_get = m.Resolve("UnityEngine", "Rigidbody", "get_detectCollisions", "System.Boolean", {});
    out.rigidbody_set =
        m.Resolve("UnityEngine", "Rigidbody", "set_detectCollisions", "System.Void", {"System.Boolean"});
    out.active = m.Resolve("UnityEngine", "GameObject", "SetActive", "System.Void", {"System.Boolean"});
    out.lifetime = ResolveUnityLifetimeMethods(m);
    return out;
}
namespace detail {
ObjectResult ResolveProjectilePhysicsType(ManagedObjects& objects, ProjectilePhysicsComponent role) {
    return objects.ManagedType("UnityEngine", role == ProjectilePhysicsComponent::Rigidbody ? "Rigidbody" : "Collider");
}
}
}
