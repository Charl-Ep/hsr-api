#include "TimelineTasks.hpp"
#include "../../../adapters/presentation/AnimationHookBridge.hpp"
#include "../../../adapters/presentation/AnimationExecutionCalls.hpp"
#include "../../../adapters/presentation/TimelineTaskBindings.hpp"
#include "../../platform/NativeHooks.hpp"
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <Windows.h>
#include <array>
#include <atomic>
#include <mutex>

namespace hksr::api::detail {
struct TimelineTasks::State {
    struct Router {
        std::array<NativeHooks, TimelineTaskCount> hooks;
        std::array<std::size_t, 32> original_indices{};
        std::array<bool, TimelineTaskCount> supported{};
        std::mutex install;
        std::shared_ptr<State> current;
        bool installed = false;
        bool Pending() const {
            for (const auto& group : hooks)
                if (group.Pending())
                    return true;
            return false;
        }
        void* Original(std::size_t logical) const {
            return hooks[static_cast<std::size_t>(TimelineEntryTask(logical))].Original(original_indices[logical]);
        }
    };
    static Router& Route() {
        static auto* router = new Router;
        return *router;
    }
    Method caster;
    ReferenceMethods references;
    Identify identify;
    Before before;
    After after;
    Trace trace;
    TimelineTaskBindings binding;
    std::array<std::size_t, static_cast<std::size_t>(TimelineTask::Count)> contexts{};
    std::atomic<DWORD> thread{0};
    struct SourceEntry {
        ManagedRootId weak = 0;
        std::uint64_t epoch = 0;
        std::optional<PresentationOwner> owner;
        bool known = false;
    };
    ManagedWeakReferences weak;
    std::map<void*, SourceEntry> sources;
    std::vector<ManagedRootId> unresolved;
    std::uint64_t sequence = 0;
    struct Invocation {
        std::shared_ptr<State> state;
        TimelineTaskCall call;
        ManagedRoots roots;
        std::vector<ManagedRootId> pins;
        std::unique_ptr<PresentationCallScope> scope;
        void* receipt = nullptr;
        bool entered = false;
        std::uint64_t source_epoch = 0;
        Invocation(std::shared_ptr<State> s, TimelineTaskCall c)
            : state(std::move(s)), call(c), roots(state->references) {}
        void Keep(void* p) {
            if (!p)
                return;
            pins.reserve(pins.size() + 1);
            const auto id = roots.Keep(p);
            pins.push_back(id);
            if (!roots.Status(id).ready)
                throw std::runtime_error("task source GC retention failed");
        }
        bool Close() {
            scope.reset();
            bool complete = true;
            for (auto id : pins) {
                if (roots.Release(id).complete)
                    roots.Forget(id);
                else
                    complete = false;
            }
            pins.clear();
            return complete;
        }
    };
    State(MethodResolver& m, ReferenceMethods refs, const TimelineRegistryAccess::Fields& fields, Identify who,
          Before b, After a, Trace log)
        : references(refs), identify(std::move(who)), before(std::move(b)), after(std::move(a)), trace(std::move(log)),
          weak(refs) {
        binding = ResolveTimelineTaskBindings(m, fields);
        caster = binding.caster;
        contexts = binding.contexts;
        for (const auto& error : binding.errors)
            Log(error);
    }
    void Log(const std::string& text) noexcept {
        try {
            if (trace)
                trace("Timeline task scope: " + text);
        } catch (...) {
        }
    }
    void Forget(void* object) {
        const auto i = sources.find(object);
        if (i == sources.end())
            return;
        if (i->second.weak) {
            if (weak.Release(i->second.weak).complete)
                weak.Forget(i->second.weak);
            else {
                unresolved.push_back(i->second.weak);
                Log("source weak release unresolved");
            }
        }
        sources.erase(i);
    }
    bool Alive(void* object, const SourceEntry& entry) {
        if (!entry.weak)
            throw std::runtime_error("task source weak allocation unresolved");
        const auto result = weak.Target(entry.weak);
        if (!result.call.returned)
            throw std::runtime_error("task source weak target unavailable");
        if (result.object && result.object != object)
            throw std::runtime_error("task source identity changed");
        return result.object != nullptr;
    }
    std::optional<PresentationOwner> Source(Invocation& inv) {
        inv.Keep(inv.call.object);
        // Immediate tasks only need the current invocation. Do not keep a
        // weak cycle record waiting for a Reset/Dispose we do not intercept.
        if (!TimelineTaskRetainsCycle(inv.call.task))
            return ReadSource(inv);
        auto found = sources.find(inv.call.object);
        if (found != sources.end() &&
            (inv.call.step == TimelineTaskStep::Begin || !Alive(inv.call.object, found->second))) {
            Forget(inv.call.object);
            found = sources.end();
        }
        if (found != sources.end()) {
            inv.source_epoch = found->second.epoch;
            if (!found->second.known)
                throw std::runtime_error("task cycle source remains unknown");
            return found->second.owner;
        }
        auto slot = sources.emplace(inv.call.object, SourceEntry{}).first;
        slot->second.epoch = ++sequence;
        inv.source_epoch = slot->second.epoch;
        slot->second.weak = weak.Watch(inv.call.object);
        if (!weak.Status(slot->second.weak).ready)
            throw std::runtime_error("task source weak allocation failed");
        slot->second.owner = ReadSource(inv);
        slot->second.known = true;
        return slot->second.owner;
    }
    std::optional<PresentationOwner> ReadSource(Invocation& inv) {
        void* context = nullptr;
        if (!inv.call.object || !native::CopyMemoryChecked(&context,
                                                           static_cast<unsigned char*>(inv.call.object) +
                                                               contexts[static_cast<std::size_t>(inv.call.task)],
                                                           sizeof(context)))
            throw std::runtime_error("declared task context unreadable");
        if (!context)
            throw std::runtime_error("task context is null");
        inv.Keep(context);
        GameEntity* entity = nullptr;
        if (!native::Call(caster, &entity, context).returned)
            throw std::runtime_error("task caster getter failed");
        if (!entity)
            throw std::runtime_error("task caster absent; no established source");
        inv.Keep(entity);
        const auto origin = identify(entity);
        const auto token = origin.token;
        if (!token.entity && !token.generation && !origin.world)
            return {};
        if (token.entity != entity || !token.generation || !origin.world)
            throw std::runtime_error("owned task provenance disagrees");
        // The original world may already be disposed or detached. Its saved
        // identity partitions queries; we never invoke or root that pointer.
        return PresentationOwner{reinterpret_cast<std::uintptr_t>(origin.world), reinterpret_cast<std::uintptr_t>(entity),
                             token.generation};
    }
    static Invocation* Enter(TimelineTaskCall call) noexcept {
        const auto state = std::atomic_load(&Route().current);
        if (!state || state->thread.load() != GetCurrentThreadId() ||
            !Route().supported[static_cast<std::size_t>(call.task)])
            return nullptr;
        std::unique_ptr<Invocation> inv;
        try {
            inv = std::make_unique<Invocation>(state, call);
            // Install a barrier BEFORE any getter/callback may reenter.
            inv->scope = std::make_unique<PresentationCallScope>(std::nullopt, false);
            const auto source = state->Source(*inv);
            inv->scope.reset();
            inv->scope = std::make_unique<PresentationCallScope>(source);
        } catch (const std::exception& e) {
            state->Log(e.what());
        } catch (...) {
            state->Log("source read failed");
        }
        if (!inv)
            return nullptr;
        try {
            if (state->before) {
                inv->receipt = state->before(call);
                inv->entered = true;
            }
        } catch (const std::exception& e) {
            state->Log(std::string("consumer before failed: ") + e.what());
        } catch (...) {
            state->Log("consumer before failed");
        }
        return inv.release();
    }
    static void Leave(Invocation* raw, bool abnormal) noexcept {
        if (!raw)
            return;
        std::unique_ptr<Invocation> inv(raw);
        try {
            if (inv->entered)
                inv->state->after(inv->receipt, inv->call, abnormal);
        } catch (const std::exception& e) {
            inv->state->Log(std::string("consumer after failed: ") + e.what());
        } catch (...) {
            inv->state->Log("consumer after failed");
        }
        try {
            if (!abnormal &&
                (inv->call.step == TimelineTaskStep::Reset || inv->call.step == TimelineTaskStep::Dispose)) {
                const auto i = inv->state->sources.find(inv->call.object);
                if (i != inv->state->sources.end() && i->second.epoch == inv->source_epoch)
                    inv->state->Forget(inv->call.object);
            }
        } catch (...) {
            inv->state->Log("task source retirement failed");
        }
        try {
            if (!inv->Close()) {
                inv->state->Log("source GC cleanup unresolved; receipts retained");
                inv.release();
            }
        } catch (...) {
            inv->scope.reset();
            inv->state->Log("source cleanup exception; receipts retained");
            inv.release();
        }
    }
    template <TimelineTask Task, TimelineTaskStep Step, std::size_t Index> static void Void(void* object, void* info) {
        auto* inv = Enter({Task, Step, object});
        __try {
            ForwardAnimationVoid(Route().Original(Index), object, info);
        } __finally {
            Leave(inv, AbnormalTermination() != FALSE);
        }
    }
    template <TimelineTask Task, std::size_t Index> static void Tick(void* object, float delta, void* info) {
        auto* inv = Enter({Task, TimelineTaskStep::Tick, object, delta});
        __try {
            ForwardAnimationTick(Route().Original(Index), object, delta, info);
        } __finally {
            Leave(inv, AbnormalTermination() != FALSE);
        }
    }
};
struct TimelineTasks::Holder {
    std::shared_ptr<State> state;
};
TimelineTasks::TimelineTasks(MethodResolver& m, ReferenceMethods refs, TimelineRegistryAccess::Fields fields,
                             Identify identify, Before before, After after, Trace trace) {
    if (!identify || bool(before) != bool(after))
        throw std::invalid_argument("Timeline task needs source and paired consumer callbacks");
    auto& r = State::Route();
    std::lock_guard<std::mutex> lock(r.install);
    if (std::atomic_load(&r.current))
        throw std::logic_error("Timeline task scopes already owned");
    if (r.Pending() && !r.installed)
        throw std::logic_error("Timeline task hook installation incomplete");
    auto state = std::make_shared<State>(m, refs, fields, std::move(identify), std::move(before), std::move(after),
                                         std::move(trace));
    if (!r.installed) {
        using T = TimelineTask;
        using S = TimelineTaskStep;
        const auto& entries = state->binding.entries;
        const std::array<HookSpec, 32> specs{
            {{"Timeline Play begin", entries[0].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::Play, S::Begin, 0>>)},
             {"Timeline Stop begin", entries[1].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::Stop, S::Begin, 1>>)},
             {"Timeline Wait begin", entries[2].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::Wait, S::Begin, 2>>)},
             {"Timeline Wait tick", entries[3].code,
              reinterpret_cast<void*>(&AnimationTickEntry<&State::Tick<T::Wait, 3>>)},
             {"Timeline Wait reset", entries[4].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::Wait, S::Reset, 4>>)},
             {"Timeline Wait dispose", entries[5].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::Wait, S::Dispose, 5>>)},
             {"Timeline Finish begin", entries[6].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::WaitFinish, S::Begin, 6>>)},
             {"Timeline Finish tick", entries[7].code,
              reinterpret_cast<void*>(&AnimationTickEntry<&State::Tick<T::WaitFinish, 7>>)},
             {"Timeline Finish reset", entries[8].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::WaitFinish, S::Reset, 8>>)},
             {"Timeline Finish dispose", entries[9].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::WaitFinish, S::Dispose, 9>>)},
             {"Timeline effect sync begin", entries[10].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::EffectSync, S::Begin, 10>>)},
             {"Timeline Play tick", entries[11].code,
              reinterpret_cast<void*>(&AnimationTickEntry<&State::Tick<T::Play, 11>>)},
             {"Timeline Play reset", entries[12].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::Play, S::Reset, 12>>)},
             {"Timeline Play dispose", entries[13].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::Play, S::Dispose, 13>>)},
             {"Timeline Play skip", entries[14].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::Play, S::Skip, 14>>)},
             {"effect GFJ begin", entries[15].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::EffectConfigGFJHLACJMNK, S::Begin, 15>>)},
             {"effect NGL begin", entries[16].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::EffectConfigNGLKHNKNBDC, S::Begin, 16>>)},
             {"effect layout hide", entries[17].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::HideEffectAutoLayout, S::Begin, 17>>)},
             {"effect aim begin", entries[18].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::StartEffectAim, S::Begin, 18>>)},
             {"effect aim tick", entries[19].code,
              reinterpret_cast<void*>(&AnimationTickEntry<&State::Tick<T::StartEffectAim, 19>>)},
             {"effect aim reset", entries[20].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::StartEffectAim, S::Reset, 20>>)},
             {"effect aim dispose", entries[21].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::StartEffectAim, S::Dispose, 21>>)},
             {"effect layout scale", entries[22].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::SetEffectAutoLayoutScale, S::Begin, 22>>)},
             {"effect mahjong transfer", entries[23].code,
              reinterpret_cast<void*>(
                  &AnimationObjectEntry<&State::Void<T::TransferMahJongLayoutEffect, S::Begin, 23>>)},
             {"effect layout transfer", entries[24].code,
              reinterpret_cast<void*>(
                  &AnimationObjectEntry<&State::Void<T::TriggerAutoLayoutTransferPerform, S::Begin, 24>>)},
             {"effect material block", entries[25].code,
              reinterpret_cast<void*>(
                  &AnimationObjectEntry<&State::Void<T::SetCharacterEffectMaterialBlock, S::Begin, 25>>)},
             {"effect layout snapshot", entries[26].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::RecordAutoLayoutSnapShot, S::Begin, 26>>)},
             {"effect capture texture", entries[27].code,
              reinterpret_cast<void*>(
                  &AnimationObjectEntry<&State::Void<T::SetEffectFrameCaptureMatTex, S::Begin, 27>>)},
             {"effect follow begin", entries[28].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::StopEffectFollow, S::Begin, 28>>)},
             {"effect follow tick", entries[29].code,
              reinterpret_cast<void*>(&AnimationTickEntry<&State::Tick<T::StopEffectFollow, 29>>)},
             {"effect follow reset", entries[30].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::StopEffectFollow, S::Reset, 30>>)},
             {"effect follow dispose", entries[31].code,
              reinterpret_cast<void*>(&AnimationObjectEntry<&State::Void<T::StopEffectFollow, S::Dispose, 31>>)}}};
        for (std::size_t task = 0; task < TimelineTaskCount; ++task) {
            if (!state->binding.supported[task])
                continue;
            std::vector<HookSpec> group;
            for (std::size_t logical = 0; logical < specs.size(); ++logical)
                if (static_cast<std::size_t>(TimelineEntryTask(logical)) == task) {
                    r.original_indices[logical] = group.size();
                    group.push_back(specs[logical]);
                }
            try {
                r.hooks[task].Install(group);
                r.supported[task] = true;
            } catch (const std::exception& e) {
                state->Log(e.what());
            }
        }
        r.installed = true;
    }
    holder_ = new Holder{state};
    std::atomic_store(&r.current, std::move(state));
}
TimelineTasks::~TimelineTasks() {
    Stop();
}
void TimelineTasks::ActivateOnCurrentThread() {
    if (!holder_)
        return;
    DWORD empty = 0;
    const auto id = GetCurrentThreadId();
    if (!holder_->state->thread.compare_exchange_strong(empty, id) && empty != id)
        throw std::logic_error("Timeline tasks cannot migrate engine threads");
}
void TimelineTasks::Stop() noexcept {
    if (!holder_)
        return;
    try {
        auto& r = State::Route();
        std::lock_guard<std::mutex> lock(r.install);
        if (std::atomic_load(&r.current) == holder_->state)
            std::atomic_store(&r.current, std::shared_ptr<State>{});
        holder_->state->thread.store(0);
        if (holder_->state->sources.empty() && holder_->state->unresolved.empty()) {
            delete holder_;
            holder_ = nullptr;
        }
        // Otherwise retain receipts. Stop/destruction never call the engine.
    } catch (...) {
        holder_->state->thread.store(0);
    }
}
void TimelineTasks::Collect() {
    if (!holder_)
        return;
    auto& s = *holder_->state;
    for (auto i = s.sources.begin(); i != s.sources.end();) {
        if (s.Alive(i->first, i->second)) {
            ++i;
            continue;
        }
        auto* object = i++->first;
        s.Forget(object);
    }
}
bool TimelineTasks::HasSource(PresentationOwner owner) const {
    if (!holder_)
        return false;
    for (const auto& source : holder_->state->sources)
        if (source.second.known && source.second.owner == owner)
            return true;
    return false;
}
bool TimelineTasks::Close() {
    Stop(); // detach callbacks before releasing observations
    if (!holder_)
        return true;
    auto& s = *holder_->state;
    while (!s.sources.empty())
        s.Forget(s.sources.begin()->first);
    const bool complete = s.unresolved.empty();
    Stop();
    return complete;
}
}
