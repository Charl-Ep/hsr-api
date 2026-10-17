#pragma once
#include <hksr/metadata/MethodResolver.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace hksr::sdk_v1 {
// A registry borrow, not a resource name or an entity's RuntimeID.
struct TimelineReference {
    void* registry=nullptr;
    void* record=nullptr;
    std::int32_t playback=0;
    std::uint64_t borrow=0;
};
enum class TimelinePlaybackPhase { Preparing, Playing, Paused, Stopped, Retired, Unresolved };
struct TimelineStartResult {
    CallResult call;
    std::int32_t playback=0;
    std::vector<TimelineReference> references;
    bool attributed=false;
    std::string detail;
};
struct TimelineRequestServices {
    std::function<TimelineStartResult(const std::string&,int,double)> start;
    std::function<TimelinePlaybackPhase(TimelineReference)> observe;
    std::function<CallResult(TimelineReference)> stop;
};
struct TimelineRequestStatus {
    TimelineStartResult start;
    std::vector<TimelinePlaybackPhase> phases;
    std::vector<CallResult> stops;
    bool started=false,accepted=false,cancelled=false,drained=false;
    std::string detail;
};
// One Common Timeline playback. Start/Service run on the engine thread.
// Cancel only records intent. Official updates, not Service, advance Directors.
// Keep servicing after cancellation until actual pool return/removal or reuse.
class HKSR_API OwnedTimeline {
public:
    explicit OwnedTimeline(TimelineRequestServices);
    ~OwnedTimeline(); // no engine calls
    OwnedTimeline(const OwnedTimeline&)=delete;
    OwnedTimeline& operator=(const OwnedTimeline&)=delete;
    void Start(const std::string& path,int wrap_mode=0,double start_time=0);
    void Cancel();
    void Service();
    const TimelineRequestStatus& Status() const;
private:
    struct State;State* state_;
};
}
