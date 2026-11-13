#include <hksr/presentation/timeline/EventRecordLifetime.hpp>
#include "../../../adapters/presentation/AnimationHookBridge.hpp"
#include "../../../adapters/presentation/AnimationExecutionCalls.hpp"
#include "../../platform/NativeHooks.hpp"
#include "../../../adapters/presentation/AnimationExecutionBindings.hpp"
#include <map>
#include <mutex>
#include <stdexcept>
namespace hksr::api {
namespace {
detail::NativeHooks hooks;
struct Entry {
    EventRecordToken token;
    EventRecordEnd state{true, false, false};
    unsigned depth = 0;
};
std::map<void*, Entry> entries;
std::mutex mutex;
std::uint64_t sequence = 0;
EventRecordToken Enter(void* record) {
    std::lock_guard<std::mutex> lock(mutex);
    const auto found = entries.find(record);
    if (found == entries.end())
        return {};
    auto& entry = found->second;
    if (!entry.depth)
        entry.state.returned = false;
    entry.state.entered = true;
    ++entry.depth;
    return entry.token;
}
void Returned(EventRecordToken token) {
    std::lock_guard<std::mutex> lock(mutex);
    const auto found = entries.find(token.record);
    if (found == entries.end() || found->second.token.generation != token.generation)
        return;
    auto& entry = found->second;
    if (entry.depth && --entry.depth == 0)
        entry.state.returned = true;
}
void Dispose(void* record, void* metadata) {
    const auto token = Enter(record);
    detail::ForwardAnimationVoid(hooks.Original(0), record, metadata);
    // Preserve original exceptions. Failed/nested partial disposal must not be
    // promoted to completion, and the game call is never cancelled or retried.
    Returned(token);
}
}
EventRecordToken TrackEventRecord(void* record) {
    if (!record)
        throw std::invalid_argument("event record is null");
    std::lock_guard<std::mutex> lock(mutex);
    if (entries.count(record))
        throw std::logic_error("event record already tracked");
    const EventRecordToken token{record, ++sequence};
    entries.emplace(record, Entry{token});
    return token;
}
EventRecordEnd ObserveEventRecord(EventRecordToken token) {
    std::lock_guard<std::mutex> lock(mutex);
    const auto found = entries.find(token.record);
    if (found == entries.end() || found->second.token.generation != token.generation)
        return {};
    return found->second.state;
}
void ForgetEventRecord(EventRecordToken token) {
    std::lock_guard<std::mutex> lock(mutex);
    const auto found = entries.find(token.record);
    if (found != entries.end() && found->second.token.generation == token.generation)
        entries.erase(found);
}
void InstallEventRecordLifetime(MethodResolver& resolver) {
    const auto dispose = detail::ResolveAnimationRecordDisposal(resolver);
    hooks.Install({{"animation event record disposal", dispose.code,
                    reinterpret_cast<void*>(&detail::AnimationObjectEntry<&Dispose>)}});
}
void RemoveEventRecordLifetimeAtQuiescentPoint() {
    hooks.Remove();
}
}
