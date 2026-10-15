#pragma once
#include "TimelineTasks.hpp"
#include "TimelineRegistryTracking.hpp"

namespace hksr::sdk_v1::detail {
// Adapts saved-record lifetime only. The original Begin/Tick still owns time,
// task state and WaitTimelineFinish's PipelineCamera count delay.
class TimelineWaits {
public:
    TimelineWaits(MethodResolver&,ObjectMetadata&,ManagedObjects&,ReferenceMethods,
        TimelineRegistryAccess::Fields,TimelineRegistryTracking&,TimelineTasks::Trace);
    ~TimelineWaits();
    TimelineWaits(const TimelineWaits&)=delete;
    TimelineWaits& operator=(const TimelineWaits&)=delete;
    void* Before(const TimelineTaskCall&);
    void After(void* receipt,const TimelineTaskCall&,bool abnormal);
    void Collect(); // engine-thread: no dereference of collected task addresses
    bool Close(); // only after task callbacks are stopped
private:
    struct State;std::unique_ptr<State> state_;
};
}
