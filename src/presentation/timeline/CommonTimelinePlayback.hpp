#pragma once
#include "TimelineRegistryTracking.hpp"
#include <hksr/presentation/timeline/OwnedTimeline.hpp>
#include <hksr/entity/EntityAccess.hpp>

namespace hksr::sdk_v1::detail {
// Uses the existing global service and registry hooks. No second registry,
// task scheduler, private Director update, or global completion notification.
class CommonTimelinePlayback {
public:
    CommonTimelinePlayback(MethodResolver&,ObjectMetadata&,ManagedObjects&,ReferenceMethods,TimelineRegistryTracking&);
    ~CommonTimelinePlayback();
    TimelineStartResult Start(EntityOrigin,const std::string&,int,double);
    TimelinePlaybackPhase Observe(TimelineOwner,TimelineReference);
    CallResult Stop(TimelineOwner,TimelineReference);
    bool Close();
private:
    struct State;std::unique_ptr<State> state_;
};
}
