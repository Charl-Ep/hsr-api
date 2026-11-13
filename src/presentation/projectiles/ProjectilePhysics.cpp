#include <hksr/presentation/projectiles/ProjectilePhysics.hpp>
#include "../../../adapters/presentation/ProjectilePhysicsBindings.hpp"
#include <hksr/native/Invocation.hpp>
#include <set>
#include <stdexcept>

namespace hksr::api {
struct ProjectilePhysics::State {
    ProjectilePhysicsMethods methods;
    ManagedRoots roots;
    ManagedArrays arrays;
    ManagedObjects& objects;
    std::vector<ManagedRootId> held;
    std::set<void*> retained;
    void* wrapper = nullptr;
    bool attempted = false;
    ProjectilePhysicsStatus status;
    State(ProjectilePhysicsMethods m, ReferenceMethods r, ManagedArrays a, ManagedObjects& o)
        : methods(std::move(m)), roots(r), arrays(std::move(a)), objects(o) {}
    void* Keep(ObjectResult value) {
        if (!value.call.returned || !value.object)
            throw std::runtime_error("projectile physics object unavailable");
        if (retained.count(value.object))
            return value.object;
        held.reserve(held.size() + 1);
        held.push_back(roots.Keep(value.object));
        if (!roots.Status(held.back()).ready)
            throw std::runtime_error("projectile physics retention failed");
        retained.insert(value.object);
        return value.object;
    }
    void Disable(detail::ProjectilePhysicsComponent role, Method get, Method set, unsigned& total) {
        auto* type = Keep(detail::ResolveProjectilePhysicsType(objects, role));
        ObjectResult value;
        value.call = native::Call(methods.components, &value.object, wrapper, type, true);
        auto* list = Keep(value);
        const auto count = arrays.Length(list);
        if (!count.read.returned || count.value < 0)
            throw std::runtime_error("projectile physics component list unavailable");
        // Independent components are all processed even if one setter fails.
        // The outer handler deactivates the wrapper before returning failure.
        bool failed = false;
        for (int i = 0; i < count.value; ++i) {
            const auto item = arrays.Get(list, i);
            if (!item.call.returned) {
                failed = true;
                continue;
            }
            if (!item.object)
                continue;
            auto* component = Keep(item);
            ++total;
            bool enabled = true;
            const auto write = native::CallVoid(set, component, false);
            const auto read = native::Call(get, &enabled, component);
            if (!write.returned || !read.returned || enabled)
                failed = true;
        }
        if (failed)
            throw std::runtime_error("projectile physics disable failed");
    }
};
ProjectilePhysics::ProjectilePhysics(ProjectilePhysicsMethods m, ReferenceMethods r, ManagedArrays a, ManagedObjects& o)
    : state_(std::make_unique<State>(std::move(m), r, std::move(a), o)) {}
ProjectilePhysics::~ProjectilePhysics() = default;
const ProjectilePhysicsStatus& ProjectilePhysics::Status() const {
    return state_->status;
}
bool ProjectilePhysics::Isolate(void* wrapper) {
    auto& s = *state_;
    auto& out = s.status;
    if (s.attempted || out.closed)
        return out.isolated;
    s.attempted = true;
    if (!wrapper) {
        out.isolated = true;
        out.stopped = true;
        return true;
    } // no FlyEffect
    s.wrapper = wrapper;
    try {
        s.Keep({{true, 0}, wrapper});
        std::string failures;
        try {
            s.Disable(detail::ProjectilePhysicsComponent::Rigidbody, s.methods.rigidbody_get, s.methods.rigidbody_set,
                      out.rigidbodies);
        } catch (const std::exception& e) {
            failures = e.what();
        }
        try {
            s.Disable(detail::ProjectilePhysicsComponent::Collider, s.methods.collider_get, s.methods.collider_set,
                      out.colliders);
        } catch (const std::exception& e) {
            if (!failures.empty())
                failures += "; ";
            failures += e.what();
        }
        if (!failures.empty())
            throw std::runtime_error(failures);
        out.isolated = true;
        return true;
    } catch (const std::exception& e) {
        out.error = e.what();
        Stop();
        return false;
    }
}
bool ProjectilePhysics::Stop() {
    auto& s = *state_;
    auto& out = s.status;
    if (out.stopped || out.closed)
        return true;
    if (!s.wrapper) {
        out.stopped = true;
        return true;
    }
    const auto alive = UnityObjectAlive(s.methods.lifetime.alive, s.wrapper);
    if (!alive.read.returned) {
        out.error = "projectile wrapper lifetime unreadable";
        return false;
    }
    if (!alive.value) {
        out.stopped = true;
        return true;
    }
    out.stopped = native::CallVoid(s.methods.active, s.wrapper, false).returned;
    if (!out.stopped)
        out.error = "projectile wrapper deactivation failed";
    return out.stopped;
}
bool ProjectilePhysics::Close() {
    auto& s = *state_;
    auto& out = s.status;
    if (out.closed)
        return true;
    if (s.wrapper) {
        const auto alive = UnityObjectAlive(s.methods.lifetime.alive, s.wrapper);
        if (!alive.read.returned || alive.value)
            return false;
    }
    for (auto it = s.held.begin(); it != s.held.end();) {
        if (s.roots.Release(*it).complete) {
            s.roots.Forget(*it);
            it = s.held.erase(it);
        } else
            ++it;
    }
    out.closed = s.held.empty();
    return out.closed;
}
}
