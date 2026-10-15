#pragma once
#include "../../metadata/MethodResolver.hpp"
#include <cstdint>
namespace hksr::sdk_v1 {
struct EventRecordToken { void* record=nullptr; std::uint64_t generation=0; };
struct EventRecordEnd {
    bool tracked=false,entered=false,returned=false;
};
// Observes only registrations explicitly retained by the caller. No game
// object/GC ownership. A completed Dispose call is not a universal proof that
// all effects or scene state have been restored; it is an exact call receipt.
HKSR_API EventRecordToken TrackEventRecord(void* rooted_record);
HKSR_API EventRecordEnd ObserveEventRecord(EventRecordToken token);
HKSR_API void ForgetEventRecord(EventRecordToken token);
HKSR_API void InstallEventRecordLifetime(MethodResolver& resolver);
HKSR_API void RemoveEventRecordLifetimeAtQuiescentPoint();
}
