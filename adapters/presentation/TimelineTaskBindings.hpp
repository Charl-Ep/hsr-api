#pragma once
#include "../../src/presentation/timeline/TimelineTasks.hpp"
#include <array>
namespace hksr::api::detail {
constexpr std::size_t TimelineTaskCount = static_cast<std::size_t>(TimelineTask::Count);
struct TimelineTaskBindings {
    Method caster;
    std::array<std::size_t, TimelineTaskCount> contexts{};
    std::array<bool, TimelineTaskCount> supported{};
    std::array<Method, 32> entries{};
    std::vector<std::string> errors;
};
TimelineTaskBindings ResolveTimelineTaskBindings(MethodResolver&, const TimelineRegistryAccess::Fields&);
TimelineTask TimelineEntryTask(std::size_t logical_index);
bool TimelineTaskRetainsCycle(TimelineTask);
}
