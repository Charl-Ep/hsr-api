#include "LifecycleHookBridge.hpp"
#include "../../src/platform/NativeHooks.hpp"
#include <stdexcept>

namespace hksr::api::detail {
namespace {
NativeHooks hooks;
EntityLifecycleCallbacks callbacks;

GameEntity* __fastcall CreateEmpty(GameWorld* world, int team, int type, void* metadata) {
    const auto original = reinterpret_cast<GameEntity*(__fastcall*)(GameWorld*, int, int, void*)>(hooks.Original(0));
    auto* result = original(world, team, type, metadata);
    callbacks.created(world, result);
    return result;
}
GameEntity* __fastcall CreateNew(GameWorld* world, std::uint32_t id, int team, int type, void* metadata) {
    const auto original =
        reinterpret_cast<GameEntity*(__fastcall*)(GameWorld*, std::uint32_t, int, int, void*)>(hooks.Original(2));
    auto* result = original(world, id, team, type, metadata);
    callbacks.created(world, result);
    return result;
}
void __fastcall Dispose(GameEntity* entity, void* metadata) {
    const auto checkpoint = callbacks.disposing(entity);
    const auto original = reinterpret_cast<void(__fastcall*)(GameEntity*, void*)>(hooks.Original(1));
    original(entity, metadata);
    // Only a normally returned official Dispose completes the core receipt.
    // Neither native nor observer exceptions are rewritten by this bridge.
    callbacks.disposed(entity, checkpoint);
}
}
void InstallEntityLifecycleBridge(const std::array<Method, 3>& entries, EntityLifecycleCallbacks sink) {
    if (!sink.created || !sink.disposing || !sink.disposed)
        throw std::invalid_argument("entity lifecycle bridge needs complete semantic callbacks");
    if (hooks.Pending())
        throw std::logic_error("entity lifecycle bridge already installed or needs removal");
    // The callbacks must also survive a partially enabled hook installation.
    callbacks = sink;
    hooks.Install({{"CreateEmptyEntity", entries[0].code, reinterpret_cast<void*>(&CreateEmpty)},
                   {"GameEntity.Dispose", entries[1].code, reinterpret_cast<void*>(&Dispose)},
                   {"_CreateNewEntityImpl", entries[2].code, reinterpret_cast<void*>(&CreateNew)}});
}
void RemoveEntityLifecycleBridgeAtQuiescentPoint() {
    hooks.Remove();
    callbacks = {};
}
}
