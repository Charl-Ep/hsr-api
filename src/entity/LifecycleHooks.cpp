#include <hksr/entity/LifecycleHooks.hpp>
#include "../../adapters/entity/LifecycleBindings.hpp"
#include "../../adapters/entity/LifecycleHookBridge.hpp"
#include <stdexcept>
#include <string>
#include <Windows.h>

namespace hksr::api {
namespace {
EntityLifecycle entities;

void Created(GameWorld* world, GameEntity* entity) {
    // Also covers explicit-ID auxiliary entities that bypass CreateEmptyEntity.
    // Nested completion of the wrapper reuses this birth, not a second epoch.
    EntityCreationScope::FactoryCompleted(world, entity);
}
EntityToken Disposing(GameEntity* entity) {
    const auto token = entities.Current(entity);
    try {
        const auto creating = EntityCreationScope::DisposalStarting(entity);
        const auto notice = entities.DisposalStarting(token);
        if (creating.failed || notice.failed || notice.wrong_thread)
            OutputDebugStringA("HKSR: entity pre-disposal observer failed or ran on a different thread; forwarding "
                               "original Dispose.\n");
    } catch (...) {
        OutputDebugStringA("HKSR: entity pre-disposal dispatch failed; forwarding original Dispose.\n");
    }
    return token;
}
void Disposed(GameEntity* entity, EntityToken token) {
    entities.Disposed(token);
    EntityCreationScope::DisposalCompleted(entity);
}
}
EntityLifecycle& TrackedEntities() {
    return entities;
}
void RemoveEntityLifecycleHooksAtQuiescentPoint() {
    // Called by the host only when no invocation/trampoline is in flight.
    // Retain failed entries so an incomplete removal cannot be mistaken for
    // permission to unload the DLL. Do not globally remove other SDK hooks.
    detail::RemoveEntityLifecycleBridgeAtQuiescentPoint();
}
void InstallEntityLifecycleHooks(MethodResolver& resolver) {
    const auto entries = detail::ResolveEntityLifecycleEntries(resolver);
    detail::InstallEntityLifecycleBridge(entries, {Created, Disposing, Disposed});
}
}
