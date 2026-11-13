#include <hksr/presentation/events/AnimationSequenceExecutions.hpp>
#include "../../../adapters/presentation/AnimationHookBridge.hpp"
#include "../../../adapters/presentation/AnimationExecutionCalls.hpp"
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include "AnimationExecutionScope.hpp"
#include "../../../adapters/presentation/AnimationExecutionBindings.hpp"
#include "../../platform/NativeHooks.hpp"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <map>
#include <mutex>

namespace hksr::api {
namespace {
thread_local void* current_scope = nullptr;
bool Same(EntityOrigin a, EntityOrigin b) {
    return a.token.entity == b.token.entity && a.token.generation == b.token.generation && a.world == b.world;
}
template <class T> T Read(void* object, std::size_t offset) {
    T value{};
    if (!object || !native::CopyMemoryChecked(&value, static_cast<char*>(object) + offset, sizeof(value)))
        throw std::runtime_error("sequence input unreadable");
    return value;
}
struct SequenceScope {
    struct Entry {
        AnimationSequenceExecution value;
        const std::atomic<DWORD>* active;
    };
    std::vector<Entry> executions;
};
}
namespace detail {
void* ExchangeAnimationExecutionScope(void* scope) noexcept {
    auto* old = current_scope;
    current_scope = scope;
    return old;
}
void* CurrentAnimationExecutionScope() noexcept {
    return current_scope;
}
AnimationSequenceExecution SequenceFromScope(void* raw, EntityOrigin origin) noexcept {
    if (!raw)
        return {};
    const auto& values = static_cast<SequenceScope*>(raw)->executions;
    for (const auto& entry : values)
        if (entry.active->load() == GetCurrentThreadId() && entry.value.serial && Same(entry.value.origin, origin))
            return entry.value;
    return {};
}
}
struct AnimationSequenceExecutions::State {
    struct Router {
        std::array<detail::NativeHooks, 2> hooks;
        std::mutex install;
        std::array<void*, 7> entries{};
        bool installed = false;
        std::array<std::size_t, 7> original_indices{};
        std::array<bool, 2> supported{};
        bool Pending() const {
            for (const auto& group : hooks)
                if (group.Pending())
                    return true;
            return false;
        }
        void* Original(std::size_t logical) const {
            return hooks[static_cast<std::size_t>(detail::AnimationSequenceEntryKind(logical))].Original(
                original_indices[logical]);
        }
        using Clients = std::vector<std::shared_ptr<State>>;
        std::shared_ptr<const Clients> clients = std::make_shared<const Clients>();
    };
    static Router& Route() {
        static auto* route = new Router;
        return *route;
    }
    ObjectMetadata& metadata;
    ReferenceMethods references;
    ManagedWeakReferences weak;
    Method caster;
    Identify identify;
    Observe observe;
    Trace trace;
    std::array<std::size_t, 2> contexts{}, states{};
    std::array<bool, 2> supported{};
    std::atomic<DWORD> active{0};
    DWORD owner = 0;
    struct Definition {
        ManagedRootId weak = 0;
        std::uint64_t construction = 0, id = 0, execution = 0, parent_execution = 0;
        EntityOrigin origin;
        AnimationSequenceKind kind = AnimationSequenceKind::Persistent;
    };
    std::map<void*, std::shared_ptr<Definition>> definitions;
    std::vector<ManagedRootId> pending;
    struct Call {
        std::shared_ptr<State> state;
        std::shared_ptr<Definition> definition;
        AnimationSequenceObservation value;
        ManagedRoots roots;
        std::vector<ManagedRootId> pins;
        explicit Call(std::shared_ptr<State> s) : state(std::move(s)), roots(state->references) {}
        void Keep(void* object) {
            if (!object)
                return;
            pins.reserve(pins.size() + 1);
            pins.push_back(roots.Keep(object));
            if (!roots.Status(pins.back()).ready)
                throw std::runtime_error("sequence call retention failed");
        }
        bool Close() {
            bool complete = true;
            for (auto id : pins) {
                if (roots.Release(id).complete)
                    roots.Forget(id);
                else
                    complete = false;
            }
            return complete;
        }
    };
    struct Batch : SequenceScope {
        std::vector<std::unique_ptr<Call>> calls;
    };
    State(MethodResolver& methods, ObjectMetadata& types, ReferenceMethods refs, const Fields& fields, Identify who,
          Observe callback, Trace log)
        : metadata(types), references(refs), weak(refs), caster(detail::ResolveTaskCaster(methods)),
          identify(std::move(who)), observe(std::move(callback)), trace(std::move(log)) {
        const auto layout = detail::ResolveAnimationSequenceLayout(fields);
        contexts = layout.contexts;
        states = layout.states;
        supported = layout.supported;
        for (const auto& error : layout.errors)
            Log(error);
    }
    void Log(const std::string& text) noexcept {
        try {
            if (trace)
                trace("sequence observation: " + text);
        } catch (...) {
        }
    }
    bool ReleaseWeak(ManagedRootId id) {
        if (!weak.Release(id).complete)
            return false;
        weak.Forget(id);
        return true;
    }
    void Retire(std::map<void*, std::shared_ptr<Definition>>::iterator it) {
        const auto definition = it->second;
        AnimationSequenceObservation ended;
        ended.operation = AnimationSequenceOperation::DefinitionRetired;
        ended.execution = {definition->construction,
                           definition->id,
                           definition->execution,
                           definition->parent_execution,
                           definition->origin,
                           it->first,
                           nullptr,
                           definition->kind};
        pending.push_back(definition->weak);
        definition->execution = 0;
        definitions.erase(it);
        if (active.load() == GetCurrentThreadId())
            try {
                observe(ended);
            } catch (...) {
                Log("definition retirement callback failed");
            }
    }
    void Collect() {
        // Retirement notifications may synchronously close or change the
        // observer. Keep stable identities across callback reentry.
        const auto snapshot = definitions;
        for (const auto& [sequence, definition] : snapshot) {
            auto found = definitions.find(sequence);
            if (found == definitions.end() || found->second != definition)
                continue;
            const auto target = weak.Target(definition->weak);
            found = definitions.find(sequence);
            if (target.call.returned && !target.object && found != definitions.end() && found->second == definition)
                Retire(found);
        }
        for (auto it = pending.begin(); it != pending.end();)
            if (ReleaseWeak(*it))
                it = pending.erase(it);
            else
                ++it;
    }
    static void Release(std::unique_ptr<Call>& call) noexcept {
        if (!call)
            return;
        try {
            if (call->Close()) {
                call.reset();
                return;
            }
        } catch (...) {
        }
        call->state->Log("GC cleanup unresolved; call retained");
        call.release();
    }
    static Batch* Enter(void* sequence, AnimationSequenceKind kind, AnimationSequenceOperation operation,
                        void* parent) noexcept {
        static std::atomic<std::uint64_t> next{0};
        const auto serial = operation == AnimationSequenceOperation::Begin ? ++next : 0;
        const auto clients = std::atomic_load(&Route().clients);
        std::unique_ptr<Batch> batch;
        for (const auto& s : *clients) {
            if (s->active.load() != GetCurrentThreadId() || !s->supported[static_cast<std::size_t>(kind)] ||
                !Route().supported[static_cast<std::size_t>(kind)])
                continue;
            auto found = s->definitions.find(sequence);
            if (found == s->definitions.end())
                continue;
            auto definition = found->second;
            std::unique_ptr<Call> call;
            try {
                if (!batch) {
                    batch = std::make_unique<Batch>();
                    batch->calls.reserve(clients->size());
                    batch->executions.reserve(clients->size());
                }
                const auto target = s->weak.Target(definition->weak);
                if (!target.call.returned)
                    throw std::runtime_error("sequence identity unavailable");
                if (target.object != sequence) {
                    s->Retire(found);
                    continue;
                }
                if (definition->kind != kind)
                    throw std::runtime_error("sequence kind changed");
                call = std::make_unique<Call>(s);
                call->definition = definition;
                call->Keep(sequence);
                auto* context = Read<void*>(sequence, s->contexts[static_cast<std::size_t>(kind)]);
                call->Keep(context);
                GameEntity* entity = nullptr;
                if (!context || !native::Call(s->caster, &entity, context).returned)
                    throw std::runtime_error("sequence caster unavailable");
                call->Keep(entity);
                const auto origin = entity ? s->identify(entity) : EntityOrigin{};
                if (!Same(origin, definition->origin)) {
                    s->Retire(found);
                    Release(call);
                    continue;
                }
                if (serial) {
                    definition->execution = serial;
                    definition->parent_execution = detail::SequenceFromScope(parent, origin).serial;
                }
                call->value.execution = {definition->construction,
                                         definition->id,
                                         definition->execution,
                                         definition->parent_execution,
                                         origin,
                                         sequence,
                                         context,
                                         kind};
                call->value.operation = operation;
                call->value.entering = true;
                // Only execution methods propagate provenance. Reset/Dispose
                // mask the outer scope, including nested cleanup callbacks.
                if (operation == AnimationSequenceOperation::Begin || operation == AnimationSequenceOperation::Tick)
                    batch->executions.push_back({call->value.execution, &s->active});
                auto* ready = call.get();
                batch->calls.push_back(std::move(call));
                try {
                    s->observe(ready->value);
                } catch (...) {
                    s->Log("enter callback failed; original forwarded");
                }
            } catch (const std::exception& e) {
                s->Log(e.what());
            } catch (...) {
                s->Log("sequence source unavailable; original forwarded");
            }
            Release(call);
        }
        return batch.release();
    }
    static void Leave(Batch* raw, bool returned) noexcept {
        std::unique_ptr<Batch> batch(raw);
        if (!batch)
            return;
        for (auto& call : batch->calls) {
            auto& s = *call->state;
            auto& value = call->value;
            auto& definition = *call->definition;
            value.entering = false;
            value.original_returned = returned;
            try {
                value.task_state = Read<std::int32_t>(value.execution.sequence,
                                                      s.states[static_cast<std::size_t>(value.execution.kind)]);
                value.state_read = true;
                if (definition.execution == value.execution.serial &&
                    (!returned || value.operation == AnimationSequenceOperation::Reset ||
                     value.operation == AnimationSequenceOperation::Dispose ||
                     value.execution.kind == AnimationSequenceKind::Immediate ||
                     !detail::IsAnimationSequenceRunning(value.task_state)))
                    definition.execution = 0;
                value.continues = value.execution.serial && definition.execution == value.execution.serial;
                if (value.operation == AnimationSequenceOperation::Dispose) {
                    auto it = s.definitions.find(value.execution.sequence);
                    if (it != s.definitions.end() && it->second == call->definition)
                        s.Retire(it);
                }
            } catch (...) {
                definition.execution = 0;
                s.Log("return state unavailable; execution attribution ended");
            }
            if (s.active.load() == GetCurrentThreadId())
                try {
                    s.observe(value);
                } catch (...) {
                    s.Log("return callback failed");
                }
            Release(call);
        }
    }
    template <unsigned Index, AnimationSequenceKind Kind, AnimationSequenceOperation Operation>
    static void Entry(void* sequence, void* metadata) {
        const auto error = GetLastError();
        auto* previous = detail::ExchangeAnimationExecutionScope(nullptr);
        auto* batch = Enter(sequence, Kind, Operation, previous);
        detail::ExchangeAnimationExecutionScope(static_cast<SequenceScope*>(batch));
        SetLastError(error);
        __try {
            detail::ForwardAnimationVoid(Route().Original(Index), sequence, metadata);
        } __finally {
            const auto result_error = GetLastError();
            detail::ExchangeAnimationExecutionScope(nullptr);
            Leave(batch, AbnormalTermination() == FALSE);
            detail::ExchangeAnimationExecutionScope(previous);
            SetLastError(result_error);
        }
    }
    static void Tick(void* sequence, float delta, void* metadata) {
        const auto error = GetLastError();
        auto* previous = detail::ExchangeAnimationExecutionScope(nullptr);
        auto* batch = Enter(sequence, AnimationSequenceKind::Persistent, AnimationSequenceOperation::Tick, previous);
        detail::ExchangeAnimationExecutionScope(static_cast<SequenceScope*>(batch));
        SetLastError(error);
        __try {
            detail::ForwardAnimationTick(Route().Original(1), sequence, delta, metadata);
        } __finally {
            const auto result_error = GetLastError();
            detail::ExchangeAnimationExecutionScope(nullptr);
            Leave(batch, AbnormalTermination() == FALSE);
            detail::ExchangeAnimationExecutionScope(previous);
            SetLastError(result_error);
        }
    }
};
struct AnimationSequenceExecutions::Holder {
    std::shared_ptr<State> state;
    bool stopped = false;
};
AnimationSequenceExecutions::AnimationSequenceExecutions(MethodResolver& methods, ObjectMetadata& types,
                                                         ReferenceMethods refs, Fields fields, Identify identify,
                                                         Observe observe, Trace trace) {
    if (!fields || !identify || !observe)
        throw std::invalid_argument("sequence observation requires metadata, source and callback");
    auto state = std::make_shared<State>(methods, types, refs, fields, std::move(identify), std::move(observe),
                                         std::move(trace));
    const auto binding = detail::ResolveAnimationSequenceEntries(methods);
    const auto& entries = binding.methods;
    for (const auto& error : binding.errors)
        state->Log(error);
    auto& router = State::Route();
    std::lock_guard<std::mutex> lock(router.install);
    using Kind = AnimationSequenceKind;
    using Op = AnimationSequenceOperation;
    if (router.installed) {
        if (router.entries != entries)
            throw std::invalid_argument("sequence observer belongs to another runtime");
    } else {
        if (router.Pending())
            throw std::logic_error("sequence observation installation incomplete");
        const std::array<detail::HookSpec, 7> specs{
            {{"sequence begin", entries[0],
              reinterpret_cast<void*>(&detail::AnimationObjectEntry<&State::Entry<0, Kind::Persistent, Op::Begin>>)},
             {"sequence tick", entries[1], reinterpret_cast<void*>(&detail::AnimationTickEntry<&State::Tick>)},
             {"sequence reset", entries[2],
              reinterpret_cast<void*>(&detail::AnimationObjectEntry<&State::Entry<2, Kind::Persistent, Op::Reset>>)},
             {"sequence dispose", entries[3],
              reinterpret_cast<void*>(&detail::AnimationObjectEntry<&State::Entry<3, Kind::Persistent, Op::Dispose>>)},
             {"immediate sequence begin", entries[4],
              reinterpret_cast<void*>(&detail::AnimationObjectEntry<&State::Entry<4, Kind::Immediate, Op::Begin>>)},
             {"immediate sequence reset", entries[5],
              reinterpret_cast<void*>(&detail::AnimationObjectEntry<&State::Entry<5, Kind::Immediate, Op::Reset>>)},
             {"immediate sequence dispose", entries[6],
              reinterpret_cast<void*>(&detail::AnimationObjectEntry<&State::Entry<6, Kind::Immediate, Op::Dispose>>)}}};
        for (std::size_t kind = 0; kind < binding.supported.size(); ++kind) {
            if (!binding.supported[kind] || !state->supported[kind])
                continue;
            std::vector<detail::HookSpec> group;
            for (std::size_t logical = 0; logical < specs.size(); ++logical)
                if (static_cast<std::size_t>(detail::AnimationSequenceEntryKind(logical)) == kind) {
                    router.original_indices[logical] = group.size();
                    group.push_back(specs[logical]);
                }
            try {
                router.hooks[kind].Install(group);
                router.supported[kind] = true;
            } catch (const std::exception& e) {
                state->Log(e.what());
            }
        }
        router.entries = entries;
        router.installed = true;
    }
    auto clients = std::make_shared<State::Router::Clients>(*std::atomic_load(&router.clients));
    clients->push_back(state);
    holder_ = new Holder{state};
    std::atomic_store(&router.clients, std::shared_ptr<const State::Router::Clients>(std::move(clients)));
}
AnimationSequenceExecutions::~AnimationSequenceExecutions() {
    Stop();
    delete holder_;
}
void AnimationSequenceExecutions::ActivateOnCurrentThread() {
    auto& s = *holder_->state;
    const auto thread = GetCurrentThreadId();
    if (holder_->stopped || (s.owner && s.owner != thread))
        throw std::logic_error("sequence observer stopped or on another thread");
    s.owner = thread;
    s.active.store(thread);
}
AnimationSequenceTracking AnimationSequenceExecutions::Track(const AnimationEventSource& source,
                                                             const AnimationTaskConstruction& built) {
    auto& s = *holder_->state;
    if (s.active.load() != GetCurrentThreadId())
        throw std::logic_error("sequence Track requires the active owner thread");
    if (!built.original_returned || !built.parent || !built.tasks || !built.context || !source.serial || !built.serial)
        return AnimationSequenceTracking::Unavailable;
    const auto type = s.metadata.ReadObjectTypeName(built.parent);
    const auto kind = detail::IdentifyAnimationSequence(type);
    if (!kind || !s.supported[static_cast<std::size_t>(*kind)] ||
        !State::Route().supported[static_cast<std::size_t>(*kind)])
        return AnimationSequenceTracking::Unsupported;
    GameEntity* entity = nullptr;
    if (!native::Call(s.caster, &entity, built.context).returned || !entity)
        return AnimationSequenceTracking::Unavailable;
    const auto origin = s.identify(entity);
    if (origin.token.entity != entity || !origin.token.generation || !origin.world)
        return AnimationSequenceTracking::Unavailable;
    auto existing = s.definitions.find(built.parent);
    if (existing != s.definitions.end())
        s.Retire(existing);
    auto definition = std::make_shared<State::Definition>();
    definition->construction = source.serial;
    definition->id = built.serial;
    definition->kind = *kind;
    definition->origin = origin;
    // Allocate the cleanup receipt before acquiring a managed handle.
    s.pending.reserve(s.pending.size() + 1);
    definition->weak = s.weak.Watch(built.parent);
    s.pending.push_back(definition->weak);
    if (!s.weak.Status(definition->weak).ready)
        return AnimationSequenceTracking::Unavailable;
    s.definitions.emplace(built.parent, definition);
    s.pending.pop_back();
    return AnimationSequenceTracking::Tracked;
}
void AnimationSequenceExecutions::Collect() {
    auto& s = *holder_->state;
    if (s.owner != GetCurrentThreadId())
        throw std::logic_error("sequence collection requires the owner thread");
    s.Collect();
}
void AnimationSequenceExecutions::Stop() noexcept {
    if (!holder_ || holder_->stopped)
        return;
    holder_->state->active.store(0);
    try {
        auto& router = State::Route();
        std::lock_guard<std::mutex> lock(router.install);
        auto clients = std::make_shared<State::Router::Clients>(*std::atomic_load(&router.clients));
        clients->erase(std::remove(clients->begin(), clients->end(), holder_->state), clients->end());
        std::atomic_store(&router.clients, std::shared_ptr<const State::Router::Clients>(std::move(clients)));
        holder_->stopped = true;
    } catch (...) {
    }
}
bool AnimationSequenceExecutions::Close() {
    auto& s = *holder_->state;
    if (s.owner && s.owner != GetCurrentThreadId())
        throw std::logic_error("sequence Close requires the owner thread");
    Stop();
    while (!s.definitions.empty())
        s.Retire(s.definitions.begin());
    s.Collect();
    return holder_->stopped && s.pending.empty();
}
}
