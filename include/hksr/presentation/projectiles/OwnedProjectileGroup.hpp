#pragma once
#include "ProjectileInputs.hpp"
#include <hksr/entity/EntityLifecycle.hpp>
#include <hksr/entity/MapNpcInputs.hpp>
#include <hksr/managed/ManagedArrays.hpp>
#include <hksr/presentation/effects/PresentationEffects.hpp>
#include <functional>

namespace hksr::sdk_v1 {
using ProjectileId=std::uint64_t;
struct ProjectileLaunch {
    void* parameters=nullptr;
    std::map<std::string,float> values;
    EntityOrigin target;
    // Captured target model/world, not just a reused entity pointer.
    std::function<bool()> target_current;
    std::vector<void*> retained_inputs; // model identities compared by target_current
    bool ordered=true;
};
struct ProjectileArrival {
    ProjectileId id=0;
    EntityOrigin target;
    Position3 position,velocity;
    void* hit_effect=nullptr; // borrowed during notification
    bool align=false,attach=false;
};
struct ProjectileGroupStatus {
    bool prepared=false,cancelled=false,closed=false;
    std::size_t submitted=0,arrived=0,active=0,draining=0;
    std::vector<std::string> errors;
};
struct ProjectileGroupServices {
    MethodResolver* methods=nullptr;
    ObjectMetadata* metadata=nullptr;
    ManagedObjects* objects=nullptr;
    std::function<std::vector<DeclaredField>(const char*,const char*)> fields;
    ReferenceMethods references;
    ArrayMethods arrays;
    EntityLifecycle* lifecycle=nullptr;
    std::shared_ptr<const PresentationEffects> effects;
    std::function<bool()> owner_current;
    std::vector<void*> retained_inputs; // model identities compared by owner_current
    // Notification after native update returns. Do not re-enter this group.
    std::function<void(const ProjectileArrival&)> arrival;
    std::function<void(const std::string&)> trace;
};
// One emission group with a private, normally constructed ability context.
// Does not register an AbilityComponent/ability on an entity or execute a skill.
// Engine-thread only; caller keeps servicing after cancellation until closed.
// Preparation/submission are explicit so failed native calls retain receipts.
class OwnedProjectileGroup {
public:
    HKSR_API OwnedProjectileGroup(ProjectileGroupServices,EntityOrigin owner);
    HKSR_API ~OwnedProjectileGroup();
    OwnedProjectileGroup(const OwnedProjectileGroup&)=delete;
    OwnedProjectileGroup& operator=(const OwnedProjectileGroup&)=delete;
    HKSR_API bool Prepare();
    HKSR_API ProjectileId Fire(const ProjectileLaunch&);
    HKSR_API void Service(float delta_seconds);
    HKSR_API void Cancel();
    HKSR_API const ProjectileGroupStatus& Status() const;
private:
    struct State;std::shared_ptr<State> state_;
};
}
