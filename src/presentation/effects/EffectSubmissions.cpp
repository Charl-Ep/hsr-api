#include <hksr/presentation/effects/EffectSubmissions.hpp>
#include "../../../adapters/presentation/EffectHookBridge.hpp"
#include "../../../adapters/presentation/EffectObservationCalls.hpp"
#include "../../../adapters/presentation/EffectObservationBindings.hpp"
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include "../../platform/NativeHooks.hpp"
#include "../events/AnimationExecutionScope.hpp"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>

namespace hksr::api {
namespace {
template <class T> T Read(void* object, std::size_t offset) {
    T out{};
    if (!object || !native::CopyMemoryChecked(&out, static_cast<char*>(object) + offset, sizeof(out)))
        throw std::runtime_error("effect submission input unreadable");
    return out;
}
struct TriggerFrame {
    void* object;
    std::uint64_t serial;
    void* sequence_scope;
};
thread_local TriggerFrame* current_trigger = nullptr;
struct SubmitFrame {
    SubmitFrame* previous;
    void* manager;
    void* request;
    std::uint32_t id = 0;
    bool identified = false;
};
thread_local SubmitFrame* current_submit = nullptr;
std::atomic<std::uint64_t> next_trigger{0}, next_submission{0};
}
struct EffectSubmissions::State : detail::EffectSubmissionBindings {
    struct Router {
        std::array<detail::NativeHooks, 3> hooks;
        std::array<bool, 3> supported{};
        std::mutex install;
        bool Pending() const {
            for (const auto& hook : hooks)
                if (hook.Pending())
                    return true;
            return false;
        }
        void* Original(std::size_t logical) const {
            return hooks[logical].Original(0);
        }
        using Clients = std::vector<std::shared_ptr<State>>;
        std::shared_ptr<const Clients> clients = std::make_shared<const Clients>();
        std::array<void*, 3> entries{};
        bool installed = false;
        std::size_t id_offset = 0;
    };
    static Router& Route() {
        static auto* r = new Router;
        return *r;
    }
    ReferenceMethods references;
    Identify identify;
    Before before;
    After after;
    Trace trace;
    std::atomic<DWORD> thread{0};
    struct Invocation {
        std::shared_ptr<State> state;
        EffectSubmission submission;
        ManagedRoots roots;
        std::vector<ManagedRootId> pins;
        bool notified = false;
        explicit Invocation(std::shared_ptr<State> s) : state(std::move(s)), roots(state->references) {}
        void Keep(void* object) {
            if (!object)
                return;
            pins.reserve(pins.size() + 1);
            const auto id = roots.Keep(object);
            pins.push_back(id);
            if (!roots.Status(id).ready)
                throw std::runtime_error("effect submission retention failed");
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
    struct Batch {
        std::vector<std::unique_ptr<Invocation>> calls;
    };
    State(MethodResolver& methods, ReferenceMethods refs, const Fields& fields, Identify who, Before b, After a,
          Trace log)
        : references(refs), identify(std::move(who)), before(std::move(b)), after(std::move(a)), trace(std::move(log)) {
        static_cast<detail::EffectSubmissionBindings&>(*this) =
            detail::ResolveEffectSubmissionBindings(methods, fields);
    }
    void Log(const std::string& line) noexcept {
        try {
            if (trace)
                trace("effect submission: " + line);
        } catch (...) {
        }
    }
    static void Release(std::unique_ptr<Invocation>& call) noexcept {
        if (!call)
            return;
        try {
            if (call->Close()) {
                call.reset();
                return;
            }
        } catch (...) {
        }
        call->state->Log("GC cleanup unresolved; receipt retained");
        call.release();
    }
    static Batch* Enter(SubmitFrame& frame, TriggerFrame* trigger, EffectSubmissionKind kind) noexcept {
        const auto clients = std::atomic_load(&Route().clients);
        const auto active = std::any_of(clients->begin(), clients->end(), [](const auto& client) {
            return client->thread.load() == GetCurrentThreadId();
        });
        if (!active || !Route().supported[kind == EffectSubmissionKind::Asynchronous ? 0 : 1])
            return nullptr;
        std::unique_ptr<Batch> batch;
        try {
            frame.id = Read<std::uint32_t>(frame.request, Route().id_offset);
            frame.identified = true;
            for (auto* p = frame.previous; p; p = p->previous)
                if (p->identified && p->manager == frame.manager && p->request == frame.request && p->id == frame.id)
                    return nullptr;
            batch = std::make_unique<Batch>();
            batch->calls.reserve(clients->size());
        } catch (...) {
            for (const auto& client : *clients)
                if (client->thread.load() == GetCurrentThreadId())
                    client->Log("submission receipt unavailable; original forwarded");
            return nullptr;
        }
        const auto serial = ++next_submission;
        for (const auto& client : *clients) {
            if (client->thread.load() != GetCurrentThreadId())
                continue;
            std::unique_ptr<Invocation> call;
            try {
                auto* entity = Read<GameEntity*>(frame.request, client->caster_offset);
                if (!entity)
                    continue; // no target fallback
                call = std::make_unique<Invocation>(client);
                call->Keep(frame.manager);
                call->Keep(frame.request);
                call->Keep(entity);
                const auto origin = client->identify(entity);
                if (!origin.token.entity) {
                    Release(call);
                    continue;
                }
                if (origin.token.entity != entity || !origin.token.generation || !origin.world)
                    throw std::runtime_error("submission source generation disagrees");
                call->submission = {serial, origin, frame.manager, {frame.id, 0, frame.request}, kind, {}, {}};
                if (trigger && client->trigger_error.empty()) {
                    call->Keep(trigger->object);
                    auto* config = Read<void*>(trigger->object, client->config_offset);
                    auto* context = Read<void*>(trigger->object, client->context_offset);
                    call->Keep(config);
                    call->Keep(context);
                    GameEntity* task_caster = nullptr;
                    if (config && context && native::Call(client->caster, &task_caster, context).returned &&
                        task_caster == entity) {
                        call->submission.trigger = {trigger->serial, trigger->object, config, context};
                        call->submission.sequence = detail::SequenceFromScope(trigger->sequence_scope, origin);
                        call->Keep(call->submission.sequence.sequence);
                        call->Keep(call->submission.sequence.context);
                    } else
                        client->Log("TriggerEffect caller source unresolved; receipt has no trigger attribution");
                }
                if (Read<std::uint32_t>(frame.request, client->id_offset) != frame.id ||
                    Read<GameEntity*>(frame.request, client->caster_offset) != entity)
                    throw std::runtime_error("submission inputs changed during source resolution");
                // Keep the receipt even if a consumer partially adopts then
                // throws. Its matching After still receives the call outcome.
                auto* ready = call.get();
                batch->calls.push_back(std::move(call));
                ready->notified = true;
                client->before(ready->submission);
            } catch (const std::exception& error) {
                client->Log(error.what());
            } catch (...) {
                client->Log("observer failed; original forwarded");
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
            if (call->notified && call->state->thread.load() == GetCurrentThreadId()) {
                try {
                    call->state->after(call->submission, returned);
                } catch (const std::exception& e) {
                    call->state->Log(e.what());
                } catch (...) {
                    call->state->Log("return observer failed");
                }
            }
            Release(call);
        }
    }
    static void Trigger(void* object, void* metadata) {
        TriggerFrame frame{object, ++next_trigger, detail::CurrentAnimationExecutionScope()};
        auto* previous = current_trigger;
        current_trigger = Route().supported[2] ? &frame : nullptr;
        __try {
            detail::ForwardEffectVoid(Route().Original(2), object, metadata);
        } __finally {
            current_trigger = previous;
        }
    }
    static void Async(void* manager, void* request, void* metadata) {
        const auto incoming_error = GetLastError();
        SubmitFrame frame{current_submit, manager, request};
        current_submit = &frame;
        auto* trigger = current_trigger;
        current_trigger = nullptr;
        auto* sequence = detail::ExchangeAnimationExecutionScope(nullptr);
        auto* batch = Enter(frame, trigger, EffectSubmissionKind::Asynchronous);
        SetLastError(incoming_error);
        __try {
            detail::ForwardEffectSubmission(Route().Original(0), manager, request, metadata);
        } __finally {
            const auto outgoing_error = GetLastError();
            Leave(batch, AbnormalTermination() == FALSE);
            current_submit = frame.previous;
            current_trigger = trigger;
            detail::ExchangeAnimationExecutionScope(sequence);
            SetLastError(outgoing_error);
        }
    }
    static void* Sync(void* manager, void* request, void* metadata) {
        const auto incoming_error = GetLastError();
        SubmitFrame frame{current_submit, manager, request};
        current_submit = &frame;
        auto* trigger = current_trigger;
        current_trigger = nullptr;
        auto* sequence = detail::ExchangeAnimationExecutionScope(nullptr);
        auto* batch = Enter(frame, trigger, EffectSubmissionKind::Synchronous);
        void* result = nullptr;
        SetLastError(incoming_error);
        __try {
            result = detail::ForwardEffectCreation(Route().Original(1), manager, request, metadata);
        } __finally {
            const auto outgoing_error = GetLastError();
            Leave(batch, AbnormalTermination() == FALSE);
            current_submit = frame.previous;
            current_trigger = trigger;
            detail::ExchangeAnimationExecutionScope(sequence);
            SetLastError(outgoing_error);
        }
        return result;
    }
};
struct EffectSubmissions::Holder {
    std::shared_ptr<State> state;
};
EffectSubmissions::EffectSubmissions(MethodResolver& methods, ReferenceMethods refs, Fields fields, Identify identify,
                                     Before before, After after, Trace trace) {
    if (!identify || !before || !after)
        throw std::invalid_argument("effect submission observers require source and paired callbacks");
    auto& router = State::Route();
    std::lock_guard<std::mutex> lock(router.install);
    auto state = std::make_shared<State>(methods, refs, fields, std::move(identify), std::move(before),
                                         std::move(after), std::move(trace));
    const auto binding = detail::ResolveEffectSubmissionEntries(methods);
    const auto& entries = binding.methods;
    for (const auto& error : binding.errors)
        state->Log(error);
    if (!state->trigger_error.empty())
        state->Log(state->trigger_error);
    if (router.installed) {
        if (router.entries != entries || router.id_offset != state->id_offset)
            throw std::invalid_argument("effect submission observer belongs to another runtime");
    } else {
        if (router.Pending())
            throw std::logic_error("effect submission installation incomplete");
        router.id_offset = state->id_offset;
        router.entries = entries;
        const std::array<detail::HookSpec, 3> specs{
            {{"effect async submission", entries[0],
              reinterpret_cast<void*>(&detail::EffectSubmissionEntry<&State::Async>)},
             {"effect synchronous submission", entries[1],
              reinterpret_cast<void*>(&detail::EffectCreationEntry<&State::Sync>)},
             {"TriggerEffect invocation", entries[2],
              reinterpret_cast<void*>(&detail::EffectObjectEntry<&State::Trigger>)}}};
        for (std::size_t i = 0; i < specs.size(); ++i) {
            if (!entries[i] || (i == 2 && !state->trigger_error.empty()))
                continue;
            try {
                router.hooks[i].Install({specs[i]});
                router.supported[i] = true;
            } catch (const std::exception& e) {
                state->Log(e.what());
            }
        }
        router.installed = true;
    }
    auto clients = std::make_shared<State::Router::Clients>(*std::atomic_load(&router.clients));
    clients->push_back(state);
    holder_ = new Holder{state};
    std::atomic_store(&router.clients, std::shared_ptr<const State::Router::Clients>(std::move(clients)));
}
EffectSubmissions::~EffectSubmissions() {
    Stop();
}
void EffectSubmissions::ActivateOnCurrentThread() {
    if (!holder_)
        return;
    DWORD empty = 0;
    const auto thread = GetCurrentThreadId();
    if (!holder_->state->thread.compare_exchange_strong(empty, thread) && empty != thread)
        throw std::logic_error("effect submissions cannot migrate engine threads");
}
void EffectSubmissions::Stop() noexcept {
    if (!holder_)
        return;
    holder_->state->thread.store(0);
    try {
        auto& router = State::Route();
        std::lock_guard<std::mutex> lock(router.install);
        auto clients = std::make_shared<State::Router::Clients>(*std::atomic_load(&router.clients));
        clients->erase(std::remove(clients->begin(), clients->end(), holder_->state), clients->end());
        std::atomic_store(&router.clients, std::shared_ptr<const State::Router::Clients>(std::move(clients)));
        delete holder_;
        holder_ = nullptr;
    } catch (...) {
    } // stopped state remains retained on allocation failure
}
}
