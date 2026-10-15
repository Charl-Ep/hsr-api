#include <hksr/entity/LifecycleHooks.hpp>
#include "../metadata/ResolveBatch.hpp"
#include "../platform/NativeHooks.hpp"
#include <stdexcept>
#include <string>

namespace hksr::sdk_v1 {
namespace {
detail::NativeHooks hooks;
EntityLifecycle entities;

GameEntity* __fastcall CreateEmpty(GameWorld* world, int team, int type, void* metadata) {
    auto original = reinterpret_cast<GameEntity*(__fastcall*)(GameWorld*,int,int,void*)>(hooks.Original(0));
    auto* result = original(world, team, type, metadata);
    EntityCreationScope::FactoryCompleted(world, result);
    return result;
}
void __fastcall Dispose(GameEntity* entity, void* metadata) {
    const auto token = entities.Current(entity);
    auto original = reinterpret_cast<void(__fastcall*)(GameEntity*,void*)>(hooks.Original(1));
    original(entity, metadata);
    // No catch of the original: a failed Dispose is not completed disposal.
    entities.Disposed(token);
    EntityCreationScope::DisposalCompleted(entity);
}
}
EntityLifecycle& TrackedEntities() { return entities; }
void RemoveEntityLifecycleHooksAtQuiescentPoint() {
    // Called by the host only when no invocation/trampoline is in flight.
    // Retain failed entries so an incomplete removal cannot be mistaken for
    // permission to unload the DLL. Do not globally remove other SDK hooks.
    hooks.Remove();
}
void InstallEntityLifecycleHooks(MethodResolver& resolver) {
    detail::ResolveBatch batch(resolver);
    const auto create = batch.Resolve("RPG.GameCore","GameWorld","CreateEmptyEntity","RPG.GameCore.GameEntity",
        {"RPG.GameCore.TeamType","RPG.GameCore.EntityType"});
    const auto dispose = batch.Resolve("RPG.GameCore","GameEntity","Dispose","System.Void",{});
    batch.RequireComplete("entity lifecycle");
    hooks.Install({{"CreateEmptyEntity", create.code, reinterpret_cast<void*>(&CreateEmpty)},
        {"GameEntity.Dispose", dispose.code, reinterpret_cast<void*>(&Dispose)}});
}
}
