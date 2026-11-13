#pragma once
#include <hksr/entity/EntityLifecycle.hpp>
#include <array>

namespace hksr::api::detail {
// Semantic lifecycle observations. Native team/type arguments and MethodInfo
// stay inside the adapter; the core owns the returned disposal checkpoint.
struct EntityLifecycleCallbacks {
    void (*created)(GameWorld*, GameEntity*) = nullptr;
    EntityToken (*disposing)(GameEntity*) = nullptr;
    void (*disposed)(GameEntity*, EntityToken) = nullptr;
};
void InstallEntityLifecycleBridge(const std::array<Method, 3>&, EntityLifecycleCallbacks);
void RemoveEntityLifecycleBridgeAtQuiescentPoint();
}
