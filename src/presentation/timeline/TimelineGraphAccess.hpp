#pragma once
#include "TimelineRegistryAccess.hpp"

namespace hksr::sdk_v1::detail {
// Unboxed x64 payload, NOT a managed object and NOT a boxed adjustor receiver.
// The declaration adapter checks boxed field offsets before using this ABI.
struct PlayableToken {
    std::uintptr_t handle=0;
    std::uint32_t version=0;
    std::uint32_t padding=0;
    bool operator==(const PlayableToken& b) const {return handle==b.handle && version==b.version;}
};
static_assert(sizeof(PlayableToken)==16);
struct TimelineGraphView {
    PlayableToken playable,graph;
    void* resolver=nullptr;
    bool resolver_is_director=false;
};
// Read the graph passed to a Behaviour callback via the injected BYREF APIs.
// Does not start/evaluate a graph or infer an owner from the current task.
// Resolver references last through Close; graph handles are not GC objects.
class TimelineGraphAccess {
public:
    HKSR_API TimelineGraphAccess(MethodResolver&,ObjectMetadata&,ReferenceMethods,TimelineRegistryAccess::Fields);
    HKSR_API ~TimelineGraphAccess();
    TimelineGraphAccess(const TimelineGraphAccess&)=delete;
    TimelineGraphAccess& operator=(const TimelineGraphAccess&)=delete;
    HKSR_API TimelineGraphView Read(const void* unboxed_playable);
    HKSR_API bool Close();
private:
    struct State;std::unique_ptr<State> state_;
};
}
