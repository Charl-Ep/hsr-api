#pragma once
#include "TimelineRegistryAccess.hpp"
#include "TimelineRegistryHooks.hpp"

namespace hksr::sdk_v1::detail {
struct TimelinePlaybackSource {TimelineTicket ticket;std::optional<TimelineOwner> owner;};
// Runtime registry half of the SDK's process-shared presentation isolation.
// No environment track filtering, alternate pool, timer or update loop here.
class TimelineRegistryTracking {
public:
    using Trace=TimelineRegistryHooks::Trace;
    TimelineRegistryTracking(MethodResolver&,ObjectMetadata&,ManagedObjects&,ReferenceMethods,
        TimelineRegistryAccess::Fields,Trace);
    ~TimelineRegistryTracking();
    TimelineRegistryTracking(const TimelineRegistryTracking&)=delete;
    TimelineRegistryTracking& operator=(const TimelineRegistryTracking&)=delete;
    void ActivateOnCurrentThread();
    const TimelineOwnership& Ownership() const;
    // Read a retained record's exact current registry relation; no RuntimeID or
    // text matching. Used by wait consumers after a filtered query saved it.
    std::optional<TimelineRecordView> ObserveRecord(void* record);
    // Graph resolver -> exact retained registry dictionary -> current borrow.
    // No match is UNKNOWN, not official. Does not use the surrounding task or
    // menu, and refuses Available, returned, incomplete and reused records.
    std::optional<TimelinePlaybackSource> ObserveDirector(void* director);
    // At an engine-thread quiescent point, AFTER collecting weak task/entity
    // observations. alive includes both provenance and cached task sources.
    // Actor disposal alone is not retirement while any playback is pending.
    std::size_t CollectHistories(const std::function<bool(TimelineOwner)>& alive);
    // Explicit closure after owned playbacks and consumers retire. Roots are
    // released on this engine thread, not from a worker/destructor.
    bool Close();
private:
    struct State;std::unique_ptr<State> state_;
    std::unique_ptr<TimelineRegistryHooks> hooks_;
};
}
