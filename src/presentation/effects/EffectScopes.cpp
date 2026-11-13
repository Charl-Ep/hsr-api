#include "EffectScopes.hpp"
#include "../../../adapters/presentation/EffectHookBridge.hpp"
#include "../../../adapters/presentation/EffectObservationCalls.hpp"
#include "../../../adapters/presentation/EffectObservationBindings.hpp"
#include "../../../adapters/presentation/EffectSchemaBindings.hpp"
#include <hksr/native/MetadataReader.hpp>
#include <hksr/native/Invocation.hpp>
#include "../../platform/NativeHooks.hpp"
#include <hksr/managed/ManagedRoots.hpp>
#include <Windows.h>
#include <array>
#include <atomic>
#include <mutex>

namespace hksr::api::detail {
namespace {
template <class T> T Read(void* object, std::size_t offset) {
    T value{};
    if (!object || !native::CopyMemoryChecked(&value, static_cast<unsigned char*>(object) + offset, sizeof(value)))
        throw std::runtime_error("effect scope field unreadable");
    return value;
}
}
struct EffectScopes::State : EffectScopeBindings {
    struct Router {
        std::array<NativeHooks, 3> hooks;
        std::array<std::size_t, 8> original_indices{};
        std::array<bool, 3> supported{};
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
            return hooks[EffectScopeEntryGroup(logical)].Original(original_indices[logical]);
        }
    };
    static Router& Route() {
        static auto* r = new Router;
        return *r;
    }
    ReferenceMethods refs;
    EffectRelations relations;
    EffectQueries::Source source;
    EffectQueries::Trace trace;
    AllowMainCamera allow_camera;
    std::shared_ptr<EffectViewPolicy> view;
    std::atomic<DWORD> thread{0};
    struct Invocation {
        std::shared_ptr<State> state;
        ManagedRoots roots;
        std::vector<ManagedRootId> pins;
        std::unique_ptr<PresentationCallScope> scope;
        void* object = nullptr;
        void* effect = nullptr;
        EffectReference request{};
        std::optional<PresentationOwner> owner;
        Invocation* previous = nullptr;
        explicit Invocation(std::shared_ptr<State> s) : state(std::move(s)), roots(state->refs) {}
        void Keep(void* p) {
            if (!p)
                return;
            pins.reserve(pins.size() + 1);
            const auto id = roots.Keep(p);
            pins.push_back(id);
            if (!roots.Status(id).ready)
                throw std::runtime_error("effect scope GC retention failed");
        }
        bool Close() {
            scope.reset();
            bool complete = true;
            for (const auto id : pins) {
                if (roots.Release(id).complete)
                    roots.Forget(id);
                else
                    complete = false;
            }
            pins.clear();
            return complete;
        }
    };
    static Invocation*& Active() {
        static thread_local Invocation* value = nullptr;
        return value;
    }
    State(MethodResolver& m, ReferenceMethods r, const EffectQueries::Fields& fields, EffectQueries::Source s,
          EffectQueries::Trace t, AllowMainCamera allow, std::shared_ptr<EffectViewPolicy> v)
        : refs(r), relations(ReadEffectRelationsFields(fields)), source(std::move(s)), trace(std::move(t)),
          allow_camera(std::move(allow)), view(std::move(v)) {
        static_cast<EffectScopeBindings&>(*this) = ResolveEffectScopeBindings(m, fields);
    }
    void Log(const std::string& text) noexcept {
        try {
            if (trace)
                trace("Effect scope: " + text);
        } catch (...) {
        }
    }
    enum class OwnerKind { Behavior, Plugin, CameraController };
    static Invocation* Enter(void* object, OwnerKind kind) noexcept {
        const auto state = std::atomic_load(&Route().current);
        if (!state || state->thread.load() != GetCurrentThreadId())
            return nullptr;
        const auto group = kind == OwnerKind::Behavior ? 0u : kind == OwnerKind::Plugin ? 1u : 2u;
        if (!Route().supported[group])
            return nullptr;
        if (kind == OwnerKind::CameraController &&
            (!Route().supported[2] || !state->camera_plugin.code || !state->allow_camera))
            return nullptr;
        std::unique_ptr<Invocation> inv;
        try {
            inv = std::make_unique<Invocation>(state);
            inv->object = object;
            // Even getter/source failure must not inherit a surrounding actor.
            inv->scope = std::make_unique<PresentationCallScope>(std::nullopt, false);
            inv->Keep(object);
            void* effect = nullptr;
            if (kind == OwnerKind::CameraController) {
                void* plugin = nullptr;
                if (!object || !native::Call(state->camera_plugin, &plugin, object).returned || !plugin)
                    throw std::runtime_error("camera plugin getter failed");
                inv->Keep(plugin);
                effect = Read<void*>(plugin, state->plugin_owner);
            } else if (kind == OwnerKind::Plugin)
                effect = Read<void*>(object, state->plugin_owner);
            else if (!object || !native::Call(state->owner_mono, &effect, object).returned)
                throw std::runtime_error("effect owner getter failed");
            if (!effect)
                throw std::runtime_error("effect owner absent");
            inv->Keep(effect);
            auto* request = Read<void*>(effect, state->request_offset);
            if (!request)
                throw std::runtime_error("effect request absent");
            inv->Keep(request);
            const EffectReference ref{Read<std::uint32_t>(request, state->id_offset), 0, request};
            const auto facts = state->relations.Read(ref);
            if (!facts.same_generation || !facts.caster.call.returned)
                throw std::runtime_error("effect source unreadable");
            inv->Keep(facts.caster.object);
            const auto owner = state->source(ref, facts);
            if (Read<void*>(effect, state->request_offset) != request ||
                Read<std::uint32_t>(request, state->id_offset) != ref.id)
                throw std::runtime_error("effect request changed during source resolution");
            inv->effect = effect;
            inv->request = ref;
            inv->owner = owner;
            inv->scope.reset();
            inv->scope = std::make_unique<PresentationCallScope>(owner);
        } catch (const std::exception& e) {
            state->Log(e.what());
        } catch (...) {
            state->Log("source resolution failed");
        }
        if (inv) {
            inv->previous = Active();
            Active() = inv.get();
        }
        return inv.release();
    }
    static void Leave(Invocation* raw) noexcept {
        if (!raw)
            return;
        Active() = raw->previous;
        std::unique_ptr<Invocation> inv(raw);
        try {
            if (!inv->Close()) {
                inv->state->Log("GC cleanup unresolved; receipts retained");
                inv.release();
            }
        } catch (...) {
            inv->scope.reset();
            inv->state->Log("GC cleanup failed; receipts retained");
            inv.release();
        }
    }
    template <std::size_t Index, bool Plugin = false> static void Void(void* object, void* info) {
        auto* inv = Enter(object, Plugin ? OwnerKind::Plugin : OwnerKind::Behavior);
        if constexpr (Index <= 2)
            if (inv && inv->state->view)
                inv->state->view->Forget(object);
        __try {
            ForwardEffectVoid(Route().Original(Index), object, info);
        } __finally {
            if constexpr (Index == 2)
                if (inv && inv->state->view)
                    inv->state->view->Forget(object);
            Leave(inv);
        }
    }
    static void Tick(void* object, float delta, void* info) {
        auto* inv = Enter(object, OwnerKind::Behavior);
        __try {
            ForwardEffectTick(Route().Original(3), object, delta, info);
        } __finally {
            Leave(inv);
        }
    }
    static bool FilterVisible(Invocation* inv, void* object, bool requested) noexcept {
        if (inv && inv->object == object && inv->state->view) {
            try {
                if (inv->owner)
                    return inv->state->view->Visible(object, inv->effect, inv->request, *inv->owner, requested);
                inv->state->view->Forget(object);
            } catch (const std::exception& e) {
                inv->state->Log(std::string("view classification failed; original forwarded: ") + e.what());
            } catch (...) {
                inv->state->Log("view classification failed; original forwarded");
            }
        }
        return requested;
    }
    static bool Reusable(Invocation* inv, void* object) noexcept {
        if (!inv || inv->object != object || !inv->effect || inv->state->thread.load() != GetCurrentThreadId())
            return false;
        try {
            void* effect = nullptr;
            return native::Call(inv->state->owner_mono, &effect, object).returned && effect == inv->effect &&
                   Read<void*>(effect, inv->state->request_offset) == inv->request.parameters &&
                   Read<std::uint32_t>(inv->request.parameters, inv->state->id_offset) == inv->request.id;
        } catch (...) {
            return false;
        }
    }
    static void Visible(void* object, bool requested, void* info) {
        // Reuse the exact current behavior's retained source. A nested official
        // effect must resolve its own identity, never inherit the caller's.
        auto* active = Active();
        const bool reuse = Reusable(active, object);
        auto* own = reuse ? nullptr : Enter(object, OwnerKind::Behavior);
        auto* inv = reuse ? active : own;
        const bool visible = FilterVisible(inv, object, requested);
        __try {
            ForwardEffectVisibility(Route().Original(7), object, visible, info);
        } __finally {
            Leave(own);
        }
    }
    static bool BlockCamera(Invocation* inv) noexcept {
        if (!inv || !PresentationCallScope::Established())
            return false;
        try {
            const auto owner = PresentationCallScope::Current();
            return owner && inv->state->allow_camera && !inv->state->allow_camera(*owner);
        } catch (...) {
            inv->state->Log("main-camera policy failed; original forwarded");
            return false;
        }
    }
    static void CameraEnabled(void* object, bool enabled, void* info) {
        // Never suppress cleanup, including a retired/cleared owner. Original
        // false is a no-op when already disabled, otherwise unsubscribes itself.
        auto* inv = enabled ? Enter(object, OwnerKind::CameraController) : nullptr;
        __try {
            ForwardEffectVisibility(Route().Original(5), object, enabled && !BlockCamera(inv), info);
        } __finally {
            Leave(inv);
        }
    }
    static void CameraUpdate(void* object, void* info) {
        auto* inv = Enter(object, OwnerKind::CameraController);
        __try {
            // The original update writes main-camera properties AFTER calling
            // enable(true), without checking the enable flag. Enable filtering
            // alone is insufficient. Preserve cleanup using the original ABI.
            if (BlockCamera(inv))
                ForwardEffectVisibility(Route().Original(5), object, false, inv->state->camera_enable_info);
            else
                ForwardEffectVoid(Route().Original(6), object, info);
        } __finally {
            Leave(inv);
        }
    }
    void* camera_enable_info = nullptr;
};
struct EffectScopes::Holder {
    std::shared_ptr<State> state;
};
EffectScopes::EffectScopes(MethodResolver& m, ReferenceMethods refs, EffectQueries::Fields fields,
                           EffectQueries::Source source, EffectQueries::Trace trace, AllowMainCamera allow,
                           std::shared_ptr<EffectViewPolicy> view) {
    if (!source)
        throw std::invalid_argument("effect scope source resolver required");
    auto& r = State::Route();
    std::lock_guard<std::mutex> lock(r.install);
    if (std::atomic_load(&r.current))
        throw std::logic_error("effect scopes already owned");
    if (r.Pending() && !r.installed)
        throw std::logic_error("effect scope installation incomplete");
    auto state = std::make_shared<State>(m, refs, fields, std::move(source), std::move(trace), std::move(allow),
                                         std::move(view));
    const auto binding = ResolveEffectScopeEntries(m);
    state->camera_enable_info = binding.methods[5].metadata;
    for (const auto& error : binding.errors)
        state->Log(error);
    if (!state->behavior_error.empty())
        state->Log(state->behavior_error);
    if (!state->plugin_error.empty())
        state->Log(state->plugin_error);
    if (!state->camera_error.empty())
        state->Log(state->camera_error);
    if (!r.installed) {
        const auto& entries = binding.methods;
        const std::array<HookSpec, 8> specs{
            {{"effect pre-reset plugins", entries[0].code,
              reinterpret_cast<void*>(&EffectObjectEntry<&State::Void<0>>)},
             {"effect reset plugins", entries[1].code, reinterpret_cast<void*>(&EffectObjectEntry<&State::Void<1>>)},
             {"effect dispose plugins", entries[2].code, reinterpret_cast<void*>(&EffectObjectEntry<&State::Void<2>>)},
             {"effect tick", entries[3].code, reinterpret_cast<void*>(&EffectTickEntry<&State::Tick>)},
             {"effect fixed CRP query", entries[4].code,
              reinterpret_cast<void*>(&EffectObjectEntry<&State::Void<4, true>>)},
             {"effect main camera enable", entries[5].code,
              reinterpret_cast<void*>(&EffectVisibilityEntry<&State::CameraEnabled>)},
             {"effect main camera update", entries[6].code,
              reinterpret_cast<void*>(&EffectObjectEntry<&State::CameraUpdate>)},
             {"effect visibility", entries[7].code, reinterpret_cast<void*>(&EffectVisibilityEntry<&State::Visible>)}}};
        for (std::size_t group = 0; group < binding.supported.size(); ++group) {
            if (!binding.supported[group] || (group == 0 && !state->owner_mono.code) ||
                (group != 0 && !state->plugin_error.empty()) || (group == 2 && !state->camera_plugin.code))
                continue;
            std::vector<HookSpec> hooks;
            for (std::size_t logical = 0; logical < specs.size(); ++logical)
                if (EffectScopeEntryGroup(logical) == group) {
                    r.original_indices[logical] = hooks.size();
                    hooks.push_back(specs[logical]);
                }
            try {
                r.hooks[group].Install(hooks);
                r.supported[group] = true;
            } catch (const std::exception& e) {
                state->Log(e.what());
            }
        }
        r.installed = true;
    }
    holder_ = new Holder{state};
    std::atomic_store(&r.current, std::move(state));
}
EffectScopes::~EffectScopes() {
    Stop();
}
void EffectScopes::ActivateOnCurrentThread() {
    if (!holder_)
        return;
    DWORD empty = 0;
    const auto id = GetCurrentThreadId();
    if (!holder_->state->thread.compare_exchange_strong(empty, id) && empty != id)
        throw std::logic_error("effect scopes cannot migrate engine threads");
}
void EffectScopes::Stop() noexcept {
    if (!holder_)
        return;
    try {
        auto& r = State::Route();
        std::lock_guard<std::mutex> lock(r.install);
        if (std::atomic_load(&r.current) == holder_->state)
            std::atomic_store(&r.current, std::shared_ptr<State>{});
        holder_->state->thread.store(0);
        delete holder_;
        holder_ = nullptr;
    } catch (...) {
        holder_->state->thread.store(0);
    }
}
}
