#pragma once
#include <hksr/managed/ManagedRoots.hpp>
#include <memory>

namespace hksr::api {
// Immutable official parameter reference, retained independently of the catalog
// traversal. Each launch creates its own private copy through ProjectileInputs.
class ProjectileTemplate {
  public:
    HKSR_API ProjectileTemplate(ReferenceMethods, void* parameters);
    HKSR_API ~ProjectileTemplate();
    ProjectileTemplate(const ProjectileTemplate&) = delete;
    ProjectileTemplate& operator=(const ProjectileTemplate&) = delete;
    HKSR_API void* Parameters() const;
    HKSR_API bool Close(); // engine thread, also after unsuccessful retention
  private:
    struct State;
    std::unique_ptr<State> state_;
};
}
