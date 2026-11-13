#pragma once
#include "../../src/presentation/timeline/TimelineGraphAccess.hpp"
#include "../../src/presentation/timeline/TimelineEnvironment.hpp"
#include <hksr/managed/ManagedLists.hpp>
#include <array>

namespace hksr::api::detail {
struct TimelineGraphBindings {
    Method graph, valid, resolver;
};
TimelineGraphBindings ResolveTimelineGraphBindings(MethodResolver&, const TimelineRegistryAccess::Fields&);
bool IsTimelineDirector(const DeclaredType&);
bool ReadPlayableToken(const void*, PlayableToken&);
struct TimelineAssetBindings {
    Method flattened, muted, marker_count;
};
TimelineAssetBindings ResolveTimelineAssetBindings(MethodResolver&);
ObjectResult ResolveTimelineAssetType(ManagedObjects&);
bool IsTimelineAsset(const DeclaredType&);
ReferenceListMethods ResolveTimelineTrackList(const DeclaredType&);
std::array<Method, 3> ResolveTimelineEnvironmentEntries(MethodResolver&, TimelineEnvironmentKind);
}
