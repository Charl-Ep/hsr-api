#pragma once
#include <hksr/metadata/MethodResolver.hpp>
#include <hksr/entity/EntityFactory.hpp>

namespace hksr::api {
struct GameWorld;
}
namespace hksr::api::detail {
using WorldLateCallback = void (*)(GameWorld*, float delta_time);
bool WorldFrameBridgePending();
void InstallWorldFrameBridge(Method late_update, WorldLateCallback completed);
void RemoveWorldFrameBridgeAtQuiescentPoint();
}
