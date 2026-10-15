#pragma once
#include <hksr/metadata/MethodResolver.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace hksr::sdk_v1::detail {
enum class TimelineBoundary { Borrow, Return, Remove, Dispose, StopName, ResourceLookup, AssetPlaying, RecentCompletion };
// Internal SDK boundary, not a separate scheduler or research probe.
// Vectors are forwarded as their existing ABI pointers; no copies/repacking.
struct TimelineBoundaryCall {
    TimelineBoundary operation;
    void* registry=nullptr;
    void* argument=nullptr; // Borrow runtime name; Return Director; Remove record
    void* resource=nullptr;
    void* asset=nullptr;
    const void* position=nullptr;
    const void* rotation=nullptr;
    // ResourceLookup/AssetPlaying. An explicitly handled null is a genuine filtered
    // miss, not permission to fall back to an unrelated official playback.
    bool handled=false;
    void* result=nullptr;
    bool boolean_result=false; // AssetPlaying/RecentCompletion; not a record pointer
    // Queries, StopName and Return may use a shallow registry view. Registry
    // identity above remains the real registry for ownership and reconciliation.
    void* receiver=nullptr;
    // A handled StopName calls the original record stop, outside Begin's
    // exception catcher. No record means a scoped miss, not original fallback.
    void* stop_record=nullptr;
    std::int32_t frame_window=0;
};
class HKSR_API TimelineRegistryHooks {
public:
    // Begin may replace Borrow's name reference, never a resource. StopName can
    // select a record for its original HFKFNFKIEON; name text need not change.
    // ResourceLookup may explicitly supply a filtered result (including null).
    // AssetPlaying accepts a boolean result; its asset-name input is unchanged.
    // Queries/StopName can use a temporary receiver with a filtered record list;
    // their original matching/state/stop implementation still executes.
    // Return/RecentCompletion can use a temporary receiver with private history;
    // other registry members and method arguments retain their identity/value.
    // Its receipt lasts through the official call, including completion callback
    // reentry. End runs on normal return AND native/managed SEH; abnormal remains
    // the original exception. Callbacks must keep managed references they retain.
    using Begin=std::function<void*(TimelineBoundaryCall&)>;
    using End=std::function<void(void* receipt,const TimelineBoundaryCall&,void* result,bool abnormal)>;
    using Trace=std::function<void(const std::string&)>;
    TimelineRegistryHooks(MethodResolver&,Begin,End,Trace);
    ~TimelineRegistryHooks();
    TimelineRegistryHooks(const TimelineRegistryHooks&)=delete;
    TimelineRegistryHooks& operator=(const TimelineRegistryHooks&)=delete;
    void ActivateOnCurrentThread();
    // Disables callbacks only; original trampolines remain. Call after consumers
    // retire, never hot-unload this DLL while installed trampolines can execute.
    void Stop() noexcept;
private:
    struct State;struct Holder;Holder* holder_=nullptr;
};
}
