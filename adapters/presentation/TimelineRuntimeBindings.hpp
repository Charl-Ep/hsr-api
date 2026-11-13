#pragma once
#include "../../src/presentation/timeline/TimelineRegistryAccess.hpp"
#include <hksr/managed/ManagedLists.hpp>
#include <array>
namespace hksr::api::detail {
enum class TimelineObject { Registry, Record };
struct TimelineRegistryBindings {
    std::size_t list = 0, dictionary = 0, director = 0, resource = 0, name = 0, id = 0, phase = 0;
};
struct CommonTimelineBindings {
    Method play, stop;
};
struct TimelineReflectedField {
    void* object = nullptr;
    Method get, set;
};
using TimelineRetain = std::function<void*(ObjectResult)>;
TimelineRegistryBindings ResolveTimelineRegistryBindings(const TimelineRegistryAccess::Fields&);
bool IsTimelineObject(const DeclaredType&, TimelineObject);
TimelinePhase DecodeTimelinePhase(std::int32_t);
Method ResolveTimelineDirectorLookup(ObjectMetadata&, void* dictionary);
ReferenceListMethods ResolveTimelineRecordList(const DeclaredType&);
CommonTimelineBindings ResolveCommonTimelineBindings(MethodResolver&);
TimelineReflectedField ResolveCommonTimelineManager(MethodResolver&, ObjectMetadata&, ManagedObjects&,
                                                    const TimelineRetain&);
enum class TimelineRegistryField { History, Records };
struct TimelineTrackingBindings {
    TimelineRegistryAccess::Fields fields;
    Method clone;
    std::size_t history_offset = 0, query_offset = 0;
};
TimelineTrackingBindings ResolveTimelineTrackingBindings(MethodResolver&, const TimelineRegistryAccess::Fields&);
TimelineReflectedField ResolveTimelineRegistryField(MethodResolver&, ObjectMetadata&, ManagedObjects&,
                                                    TimelineRegistryField, const TimelineRetain&);
Method ResolveTimelineHistoryRange(const DeclaredType&);
Method ResolveTimelineRecordRange(const DeclaredType&);
Method ResolveTimelineRecordRemove(const DeclaredType&);
struct TimelineWaitBinding {
    std::size_t offset = 0;
    bool available = false;
    std::string error;
};
std::array<TimelineWaitBinding, 2> ResolveTimelineWaitBindings(const TimelineRegistryAccess::Fields&);
TimelineReflectedField ResolveTimelineWaitField(MethodResolver&, ObjectMetadata&, ManagedObjects&, std::size_t index,
                                                const TimelineRetain&);
}
