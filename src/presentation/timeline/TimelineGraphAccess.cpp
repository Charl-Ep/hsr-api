#include "TimelineGraphAccess.hpp"
#include "../../../adapters/presentation/TimelineBindings.hpp"
#include <hksr/native/MetadataReader.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/managed/ManagedRoots.hpp>

namespace hksr::api::detail {
struct TimelineGraphAccess::State {
    ObjectMetadata& metadata;
    ManagedRoots roots;
    Method graph, valid, resolver;
    std::vector<ManagedRootId> held;
    bool closed = false;
    State(MethodResolver& m, ObjectMetadata& d, ReferenceMethods r, const TimelineRegistryAccess::Fields& fields)
        : metadata(d), roots(r) {
        const auto bindings = ResolveTimelineGraphBindings(m, fields);
        graph = bindings.graph;
        valid = bindings.valid;
        resolver = bindings.resolver;
    }
};
TimelineGraphAccess::TimelineGraphAccess(MethodResolver& m, ObjectMetadata& d, ReferenceMethods r,
                                         TimelineRegistryAccess::Fields f)
    : state_(std::make_unique<State>(m, d, r, f)) {}
TimelineGraphAccess::~TimelineGraphAccess() {
    if (!state_->held.empty())
        state_.release();
}
TimelineGraphView TimelineGraphAccess::Read(const void* playable) {
    auto& s = *state_;
    if (s.closed)
        throw std::logic_error("Timeline graph reader already closed");
    TimelineGraphView out;
    if (!ReadPlayableToken(playable, out.playable))
        throw std::runtime_error("Playable callback value unreadable");
    // Copy prevents an injected implementation from modifying the callback's
    // original by-value temporary; padding never participates in identity.
    auto local = out.playable;
    if (!native::CallVoid(s.graph, &local, &out.graph).returned)
        throw std::runtime_error("Playable graph lookup failed");
    bool valid = false;
    if (!native::Call(s.valid, &valid, &out.graph).returned || !valid)
        throw std::runtime_error("Playable graph invalid/unreadable");
    if (!native::Call(s.resolver, &out.resolver, &out.graph).returned)
        throw std::runtime_error("Playable graph resolver lookup failed");
    if (out.resolver) {
        s.held.reserve(s.held.size() + 1);
        const auto id = s.roots.Keep(out.resolver);
        s.held.push_back(id);
        if (!s.roots.Status(id).ready)
            throw std::runtime_error("Playable resolver retention failed");
        const auto type = s.metadata.ReadObjectType(out.resolver);
        out.resolver_is_director = IsTimelineDirector(type);
    }
    return out;
}
bool TimelineGraphAccess::Close() {
    auto& s = *state_;
    s.closed = true;
    for (auto i = s.held.begin(); i != s.held.end();) {
        if (s.roots.Release(*i).complete) {
            s.roots.Forget(*i);
            i = s.held.erase(i);
        } else
            ++i;
    }
    return s.held.empty();
}
}
