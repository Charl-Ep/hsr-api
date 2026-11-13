#include <hksr/presentation/projectiles/ProjectileTemplate.hpp>
#include <stdexcept>

namespace hksr::api {
struct ProjectileTemplate::State {
    ManagedRoots roots;
    ManagedRootId id = 0;
    bool closing = false;
    State(ReferenceMethods references, void* parameters) : roots(references) {
        id = roots.Keep(parameters);
    }
};
ProjectileTemplate::ProjectileTemplate(ReferenceMethods references, void* parameters)
    : state_(std::make_unique<State>(references, parameters)) {}
ProjectileTemplate::~ProjectileTemplate() = default;
void* ProjectileTemplate::Parameters() const {
    auto& s = *state_;
    if (s.closing || !s.id || !s.roots.Status(s.id).ready)
        throw std::runtime_error("projectile template is not retained");
    const auto value = s.roots.Target(s.id);
    if (!value.call.returned || !value.object)
        throw std::runtime_error("projectile template reference unreadable");
    return value.object;
}
bool ProjectileTemplate::Close() {
    auto& s = *state_;
    s.closing = true;
    if (!s.id)
        return true;
    if (!s.roots.Release(s.id).complete)
        return false;
    s.roots.Forget(s.id);
    s.id = 0;
    return true;
}
}
