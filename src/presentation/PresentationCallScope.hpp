#pragma once
#include <hksr/Export.hpp>
#include <cstdint>
#include <optional>
#include <tuple>

namespace hksr::api::detail {
struct PresentationOwner {
    std::uintptr_t world = 0, entity = 0;
    std::uint64_t lifetime = 0;
    bool operator==(const PresentationOwner& b) const {
        return std::tie(world, entity, lifetime) == std::tie(b.world, b.entity, b.lifetime);
    }
};

// Every recognized invocation establishes its source, including official calls.
// An unknown or official nested call must not inherit an outer private actor.
// Native detours explicitly destroy the scope in SEH finally blocks.
class PresentationCallScope {
  public:
    HKSR_API explicit PresentationCallScope(std::optional<PresentationOwner> owner, bool established = true);
    HKSR_API ~PresentationCallScope();
    PresentationCallScope(const PresentationCallScope&) = delete;
    PresentationCallScope& operator=(const PresentationCallScope&) = delete;
    HKSR_API static std::optional<PresentationOwner> Current();
    HKSR_API static bool Established();
    HKSR_API static bool Active();
    HKSR_API static void ExhaustResourceQueries();
    HKSR_API static bool ResourceQueriesAllowed();

  private:
    static thread_local PresentationCallScope* current_;
    PresentationCallScope* previous_;
    std::optional<PresentationOwner> owner_;
    bool established_;
    bool resource_queries_ = true;
};
}
