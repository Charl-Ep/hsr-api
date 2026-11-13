#include "TimelineEnvironment.hpp"
#include "../../../adapters/presentation/TimelineHookBridge.hpp"
#include "../../../adapters/presentation/TimelineCalls.hpp"
#include "../../../adapters/presentation/TimelineBindings.hpp"
#include "../../platform/NativeHooks.hpp"
#include <hksr/native/MetadataReader.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <Windows.h>
#include <array>
#include <atomic>
#include <mutex>
#include <sstream>
#include <algorithm>

namespace hksr::api::detail {
struct TimelineEnvironment::State {
    struct Router {
        std::array<NativeHooks, 3> hooks;
        std::mutex install;
        std::shared_ptr<State> current;
        bool installed = false;
    };
    static Router& Route() {
        static auto* r = new Router;
        return *r;
    }
    MethodResolver& methods;
    ObjectMetadata& metadata;
    ReferenceMethods refs;
    TimelineRegistryAccess::Fields fields;
    Source source;
    Allow allow;
    Trace trace;
    ManagedWeakReferences weak;
    std::atomic<DWORD> thread{0};
    struct Cycle {
        ManagedRootId identity = 0;
        PlayableToken playable;
        bool blocked = false;
        std::uint64_t generation = 0;
    };
    std::map<std::pair<void*, TimelineEnvironmentKind>, Cycle> cycles;
    std::vector<ManagedRootId> unresolved;
    std::uint64_t sequence = 0;
    State(MethodResolver& m, ObjectMetadata& d, ReferenceMethods r, TimelineRegistryAccess::Fields f, Source s, Allow a,
          Trace t)
        : methods(m), metadata(d), refs(r), fields(std::move(f)), source(std::move(s)), allow(std::move(a)),
          trace(std::move(t)), weak(r) {}
    void Log(const std::string& s) noexcept {
        try {
            if (trace)
                trace("Timeline environment: " + s);
        } catch (...) {
        }
    }
    void Release(ManagedRootId id) {
        if (weak.Release(id).complete)
            weak.Forget(id);
        else {
            unresolved.push_back(id);
            Log("weak cleanup unresolved; receipt retained");
        }
    }
    bool Block(void* object, const void* playable, TimelineEnvironmentKind kind, bool start) {
        PlayableToken value{};
        if (!object || !ReadPlayableToken(playable, value))
            throw std::runtime_error("callback identity unreadable");
        const auto key = std::make_pair(object, kind);
        auto found = cycles.find(key);
        if (found != cycles.end()) {
            if (start)
                found->second.blocked = false;
            const auto target = weak.Target(found->second.identity);
            if (!target.call.returned)
                throw std::runtime_error("callback weak identity unreadable");
            if (target.object != object) {
                Release(found->second.identity);
                cycles.erase(found);
                found = cycles.end();
            }
        }
        if (!start) {
            // Late installation/missing start must preserve the original state
            // it may already have written. Never derive a new deny at exit.
            return found != cycles.end() && found->second.playable == value && found->second.blocked;
        }
        // Publish FORWARD first. Resolution failure after an old blocked cycle
        // must not suppress the cleanup of this newly forwarded official start.
        if (found == cycles.end()) {
            const auto id = weak.Watch(object);
            if (!weak.Status(id).ready) {
                unresolved.push_back(id);
                throw std::runtime_error("callback weak retention failed");
            }
            try {
                found = cycles.emplace(key, Cycle{id, value, false}).first;
            } catch (...) {
                Release(id);
                throw;
            }
        } else {
            found->second.playable = value;
            found->second.blocked = false;
        }
        const auto generation = ++sequence;
        found->second.generation = generation;
        bool blocked = false;
        TimelineGraphAccess graph(methods, metadata, refs, fields);
        try {
            const auto view = graph.Read(playable);
            const auto origin = view.resolver_is_director ? source(view.resolver) : std::nullopt;
            if (origin && origin->owner)
                blocked = !allow(*origin->owner, kind);
            else if (!origin)
                Log("callback start has no current registered Director; original forwarded");
            if (trace) {
                std::ostringstream line;
                line << "start kind=" << static_cast<int>(kind) << " behaviour=" << object
                     << " playable=" << reinterpret_cast<void*>(view.playable.handle) << ":" << view.playable.version
                     << " graph=" << reinterpret_cast<void*>(view.graph.handle) << ":" << view.graph.version
                     << " resolver=" << view.resolver << " is_director=" << view.resolver_is_director
                     << " blocked=" << blocked;
                if (origin)
                    line << " registry=" << reinterpret_cast<void*>(origin->ticket.key.registry)
                         << " record=" << reinterpret_cast<void*>(origin->ticket.key.record)
                         << " playback=" << origin->ticket.playback << " borrow=" << origin->ticket.borrow
                         << " owned=" << origin->owner.has_value();
                Log(line.str());
            }
            if (!graph.Close())
                Log("graph resolver GC cleanup incomplete");
        } catch (...) {
            graph.Close();
            throw;
        }
        // A nested new cycle must not be overwritten by the older start's
        // policy result. This invocation still uses its own resolved decision.
        found = cycles.find(key);
        if (found != cycles.end() && found->second.generation == generation)
            found->second.blocked = blocked;
        return blocked;
    }
    static bool ShouldBlock(void* object, const void* playable, TimelineEnvironmentKind kind, bool start) noexcept {
        const auto s = std::atomic_load(&Route().current);
        if (!s || s->thread.load() != GetCurrentThreadId())
            return false;
        try {
            return s->Block(object, playable, kind, start);
        } catch (const std::exception& e) {
            s->Log(e.what());
        } catch (...) {
            s->Log("callback decision failed; original forwarded");
        }
        return false;
    }
    // Playable (16 bytes) and FrameData (64 bytes) arrive as x64 by-value
    // temporaries passed by address. Forward the ORIGINAL pointers unchanged.
    template <std::size_t I> static void Behaviour(void* object, const void* playable, const void* frame, void* info) {
        constexpr auto kind = I < 3 ? TimelineEnvironmentKind::CharacterLight : TimelineEnvironmentKind::SceneLight;
        if (!ShouldBlock(object, playable, kind, I % 3 == 0))
            ForwardTimelineBehaviour(Route().hooks[I / 3].Original(I % 3), object, playable, frame, info);
    }
    template <std::size_t I>
    static void Process(void* object, const void* playable, const void* frame, void* data, void* info) {
        constexpr auto kind = I < 3   ? TimelineEnvironmentKind::CharacterLight
                              : I < 6 ? TimelineEnvironmentKind::SceneLight
                                      : TimelineEnvironmentKind::AdjustLevel;
        if (!ShouldBlock(object, playable, kind, false))
            ForwardTimelineProcess(Route().hooks[I / 3].Original(I % 3), object, playable, frame, data, info);
    }
    template <std::size_t I> static void Graph(void* object, const void* playable, void* info) {
        if (!ShouldBlock(object, playable, TimelineEnvironmentKind::AdjustLevel, I == 6))
            ForwardTimelineGraph(Route().hooks[I / 3].Original(I % 3), object, playable, info);
    }
};
struct TimelineEnvironment::Holder {
    std::shared_ptr<State> state;
};
TimelineEnvironment::TimelineEnvironment(MethodResolver& m, ObjectMetadata& d, ReferenceMethods refs,
                                         TimelineRegistryAccess::Fields fields, Source source, Allow allow,
                                         Trace trace) {
    if (!source || !allow)
        throw std::invalid_argument("Timeline environment source and policy required");
    auto& r = State::Route();
    std::lock_guard<std::mutex> lock(r.install);
    if (std::atomic_load(&r.current))
        throw std::logic_error("Timeline environment already owned");
    if (std::any_of(r.hooks.begin(), r.hooks.end(), [](const auto& hooks) { return hooks.Pending(); }) && !r.installed)
        throw std::logic_error("Timeline environment installation incomplete");
    // Validate the graph ABI/bindings once before installing any callbacks.
    TimelineGraphAccess schema(m, d, refs, fields);
    schema.Close();
    auto state =
        std::make_shared<State>(m, d, refs, std::move(fields), std::move(source), std::move(allow), std::move(trace));
    if (!r.installed) {
        const auto entries =
            std::array<void*, 9>{reinterpret_cast<void*>(&TimelineBehaviourEntry<&State::Behaviour<0>>),
                                 reinterpret_cast<void*>(&TimelineProcessEntry<&State::Process<1>>),
                                 reinterpret_cast<void*>(&TimelineBehaviourEntry<&State::Behaviour<2>>),
                                 reinterpret_cast<void*>(&TimelineBehaviourEntry<&State::Behaviour<3>>),
                                 reinterpret_cast<void*>(&TimelineProcessEntry<&State::Process<4>>),
                                 reinterpret_cast<void*>(&TimelineBehaviourEntry<&State::Behaviour<5>>),
                                 reinterpret_cast<void*>(&TimelineGraphEntry<&State::Graph<6>>),
                                 reinterpret_cast<void*>(&TimelineProcessEntry<&State::Process<7>>),
                                 reinterpret_cast<void*>(&TimelineGraphEntry<&State::Graph<8>>)};
        for (std::size_t group = 0; group < 3; ++group) {
            if (r.hooks[group].Pending()) {
                r.installed = true;
                continue;
            }
            try {
                const auto kind = group == 0   ? TimelineEnvironmentKind::CharacterLight
                                  : group == 1 ? TimelineEnvironmentKind::SceneLight
                                               : TimelineEnvironmentKind::AdjustLevel;
                const auto methods = ResolveTimelineEnvironmentEntries(m, kind);
                const auto i = group * 3;
                r.hooks[group].Install({{"environment begin", methods[0].code, entries[i]},
                                        {"environment process", methods[1].code, entries[i + 1]},
                                        {"environment end", methods[2].code, entries[i + 2]}});
                r.installed = true;
            } catch (const std::exception& e) {
                state->Log("binding group " + std::to_string(group) + ": " + e.what());
            }
        }
        if (!r.installed)
            throw std::runtime_error("no timeline environment contract could be bound");
    }
    holder_ = new Holder{state};
    std::atomic_store(&r.current, std::move(state));
}
TimelineEnvironment::~TimelineEnvironment() {
    if (!holder_)
        return;
    auto& r = State::Route();
    std::lock_guard<std::mutex> lock(r.install);
    if (std::atomic_load(&r.current) == holder_->state)
        std::atomic_store(&r.current, std::shared_ptr<State>{});
    // No worker-thread GC operations. Any unclosed handles keep their receipt.
    holder_->state->thread.store(0);
    if (holder_->state->cycles.empty() && holder_->state->unresolved.empty())
        delete holder_;
}
void TimelineEnvironment::ActivateOnCurrentThread() {
    if (!holder_)
        return;
    DWORD empty = 0;
    const auto thread = GetCurrentThreadId();
    if (!holder_->state->thread.compare_exchange_strong(empty, thread) && empty != thread)
        throw std::logic_error("Timeline environment cannot migrate threads");
}
std::size_t TimelineEnvironment::Collect() {
    if (!holder_)
        return 0;
    auto& s = *holder_->state;
    if (s.thread.load() != GetCurrentThreadId())
        throw std::logic_error("environment collection needs engine thread");
    std::size_t count = 0;
    for (auto i = s.cycles.begin(); i != s.cycles.end();) {
        const auto target = s.weak.Target(i->second.identity);
        if (!target.call.returned) {
            s.Log("weak collection read failed; observation retained");
            ++i;
        } else if (target.object)
            ++i;
        else {
            s.Release(i->second.identity);
            i = s.cycles.erase(i);
            ++count;
        }
    }
    return count;
}
bool TimelineEnvironment::Close() {
    if (!holder_)
        return true;
    auto& s = *holder_->state;
    if (s.thread.load() != GetCurrentThreadId())
        throw std::logic_error("environment close needs engine thread");
    auto& r = State::Route();
    std::lock_guard<std::mutex> lock(r.install);
    if (std::atomic_load(&r.current) == holder_->state)
        std::atomic_store(&r.current, std::shared_ptr<State>{});
    for (const auto& c : s.cycles)
        s.Release(c.second.identity);
    s.cycles.clear();
    const bool complete = s.unresolved.empty();
    s.thread.store(0);
    if (complete)
        delete holder_;
    holder_ = nullptr;
    return complete;
}
}
