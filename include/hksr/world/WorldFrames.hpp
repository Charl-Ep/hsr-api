#pragma once
#include "../metadata/RuntimeMetadata.hpp"
#include "../entity/EntityFactory.hpp"

namespace hksr::sdk_v1 {
struct GameWorld;
struct WorldFrame {
    GameWorld* world = nullptr;
    float delta_time = 0;
    unsigned long thread_id = 0;
    Observed<EntityManager*> manager;
    Observed<void*> adventure_phase;
    Observed<bool> disposing, battle, paused, in_tick;
    std::uint64_t sequence=0; // delivered official-update boundary, not a timer
};
struct WorldFrameMethods {
    Method late_update, manager, disposing, battle, paused, in_tick;
    std::size_t phase_offset = 0;
    bool has_phase_offset = false;
    std::vector<std::string> binding_errors;
};
// Called synchronously on the actual GameWorld.LateUpdate thread, after the
// original returns. The frame and unrooted pointers must not be queued to UI.
// Consumers must respect thread_id; this does not promise one global thread.
using WorldFrameConsumer = void(*)(const WorldFrame&, void* context);
using WorldFrameFailure = void(*)(const char* message, void* context) noexcept;
struct WorldFrameSink { WorldFrameConsumer consume=nullptr; WorldFrameFailure failure=nullptr; void* context=nullptr; };
HKSR_API WorldFrameMethods ResolveWorldFrameMethods(MethodResolver& resolver, const std::vector<DeclaredField>& world_fields);
HKSR_API void InstallWorldFrameHook(const WorldFrameMethods& methods, WorldFrameSink sink);
HKSR_API void RemoveWorldFrameHookAtQuiescentPoint();
}
