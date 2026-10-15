#pragma once
#include "TimelineGraphAccess.hpp"
#include "TimelineRegistryTracking.hpp"

namespace hksr::sdk_v1::detail {
enum class TimelineEnvironmentKind { CharacterLight, SceneLight, AdjustLevel };
// Policy is sampled at the actual callback START, never mid-cycle. A denied
// cycle omits its setup/update/exit together. An allowed cycle always retains
// official exit scheduling, including the original end-of-frame recovery.
// No global coroutine hook, replacement Timeline asset or new scheduler.
class TimelineEnvironment {
public:
    using Source=std::function<std::optional<TimelinePlaybackSource>(void* director)>;
    using Allow=std::function<bool(TimelineOwner,TimelineEnvironmentKind)>;
    using Trace=TimelineRegistryHooks::Trace;
    HKSR_API TimelineEnvironment(MethodResolver&,ObjectMetadata&,ReferenceMethods,
        TimelineRegistryAccess::Fields,Source,Allow,Trace);
    HKSR_API ~TimelineEnvironment();
    TimelineEnvironment(const TimelineEnvironment&)=delete;
    TimelineEnvironment& operator=(const TimelineEnvironment&)=delete;
    HKSR_API void ActivateOnCurrentThread();
    // Quiescent engine thread: remove COLLECTED weak observations only. Neither
    // task disposal nor Director pool return invalidates an outstanding exit.
    HKSR_API std::size_t Collect();
    // After all managed playbacks have retired. Does not cancel/replay exits.
    HKSR_API bool Close();
private:
    struct State;struct Holder;Holder* holder_=nullptr;
};
}
