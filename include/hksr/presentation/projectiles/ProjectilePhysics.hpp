#pragma once
#include <hksr/managed/ManagedArrays.hpp>
#include <hksr/managed/ManagedObjects.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/unity/OwnedUnityObjects.hpp>

namespace hksr::sdk_v1 {
struct ProjectilePhysicsMethods {
    Method components,collider_get,collider_set,rigidbody_get,rigidbody_set,active;
    UnityLifetimeMethods lifetime;
};
HKSR_API ProjectilePhysicsMethods ResolveProjectilePhysics(MethodResolver&);
struct ProjectilePhysicsStatus {
    unsigned colliders=0,rigidbodies=0;
    bool isolated=false,stopped=false,closed=false;
    std::string error;
};
// Operates only on newly created private wrapper/effect roots supplied by the
// projectile receipt. Never supply a model, shared prefab or scene parent.
// Does not destroy or restore pooled objects: the native carrier/effect owner
// completes their cleanup; Close retains roots until those receipts end.
class ProjectilePhysics {
public:
    HKSR_API ProjectilePhysics(ProjectilePhysicsMethods,ReferenceMethods,ManagedArrays,ManagedObjects&);
    HKSR_API ~ProjectilePhysics();
    ProjectilePhysics(const ProjectilePhysics&)=delete;
    ProjectilePhysics& operator=(const ProjectilePhysics&)=delete;
    // Complete on the creation frame before yielding to Unity physics.
    HKSR_API bool Isolate(void* private_wrapper);
    HKSR_API bool Stop();
    HKSR_API bool Close();
    HKSR_API const ProjectilePhysicsStatus& Status() const;
private:
    struct State;std::unique_ptr<State> state_;
};
}
