#include "WorldFrameHookBridge.hpp"
#include "../../src/platform/NativeHooks.hpp"
#include <stdexcept>

namespace hksr::api::detail {
namespace {
NativeHooks hooks;
WorldLateCallback completed = nullptr;

void __fastcall Late(GameWorld* world, float dt, void* metadata) {
    const auto original = reinterpret_cast<void(__fastcall*)(GameWorld*, float, void*)>(hooks.Original(0));
    original(world, dt, metadata);
    completed(world, dt);
}
}
bool WorldFrameBridgePending() {
    return hooks.Pending();
}
void InstallWorldFrameBridge(Method method, WorldLateCallback callback) {
    if (!method.code || !callback)
        throw std::invalid_argument("world-frame bridge needs a target and semantic callback");
    if (hooks.Pending())
        throw std::logic_error("world-frame bridge already installed or needs removal");
    completed = callback;
    hooks.Install({{"GameWorld.LateUpdate", method.code, reinterpret_cast<void*>(&Late)}});
}
void RemoveWorldFrameBridgeAtQuiescentPoint() {
    hooks.Remove();
    completed = nullptr;
}
}
