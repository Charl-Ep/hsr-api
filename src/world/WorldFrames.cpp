#include <hksr/world/WorldFrames.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include "../../adapters/world/WorldFrameHookBridge.hpp"
#include <stdexcept>
#include <string>
#include <atomic>

namespace hksr::api {
namespace {
WorldFrameMethods bindings;
WorldFrameSink receiver;
thread_local bool delivering = false;
std::atomic<std::uint64_t> sequence{0};
struct DeliveryScope {
    DeliveryScope() {
        delivering = true;
    }
    ~DeliveryScope() {
        delivering = false;
    }
};
template <class T> Observed<T> Get(Method method, GameWorld* world) {
    Observed<T> out;
    out.read = native::Call(method, &out.value, world);
    return out;
}
void Deliver(GameWorld* world, float dt) {
    // Reentrant official updates still execute, but cannot recursively service
    // the same framework request queue from its own callback.
    if (delivering || !receiver.consume)
        return;
    DeliveryScope delivery;
    try {
        WorldFrame frame;
        frame.sequence = ++sequence;
        frame.world = world;
        frame.delta_time = dt;
        frame.thread_id = GetCurrentThreadId();
        frame.manager = Get<EntityManager*>(bindings.manager, world);
        frame.disposing = Get<bool>(bindings.disposing, world);
        frame.battle = Get<bool>(bindings.battle, world);
        frame.paused = Get<bool>(bindings.paused, world);
        frame.in_tick = Get<bool>(bindings.in_tick, world);
        if (world && bindings.has_phase_offset)
            frame.adventure_phase.read.returned = native::CopyMemoryChecked(
                &frame.adventure_phase.value, reinterpret_cast<const unsigned char*>(world) + bindings.phase_offset,
                sizeof(void*));
        // Even unready/disposing worlds are delivered. Classification and
        // which independent commands can proceed belong to the consumer.
        receiver.consume(frame, receiver.context);
    } catch (const std::exception& error) {
        if (receiver.failure)
            receiver.failure(error.what(), receiver.context);
    } catch (...) {
        if (receiver.failure)
            receiver.failure("world-frame consumer failed", receiver.context);
    }
}
}
void InstallWorldFrameHook(const WorldFrameMethods& methods, WorldFrameSink sink) {
    if (detail::WorldFrameBridgePending())
        throw std::logic_error("world-frame hook already installed");
    if (!methods.late_update.code || !sink.consume)
        throw std::invalid_argument("world hook needs a target and consumer");
    bindings = methods;
    receiver = sink;
    detail::InstallWorldFrameBridge(methods.late_update, Deliver);
}
void RemoveWorldFrameHookAtQuiescentPoint() {
    detail::RemoveWorldFrameBridgeAtQuiescentPoint();
    bindings = {};
    receiver = {};
}
}
