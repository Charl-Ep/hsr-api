#include "../../adapters/animation/TimingCalls.hpp"
#include <hksr/model/ModelAccess.hpp>
#include <hksr/animation/ActorTiming.hpp>
#include "ActorTimingNative.hpp"
#include "../../adapters/animation/TimingBindings.hpp"
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/entity/EntityAccess.hpp>
#include <hksr/entity/EntityOwner.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include "../runtime/RuntimeResident.hpp"
#include <map>
#include <algorithm>
#include <stdexcept>

namespace hksr::api {
namespace {
template <class T> T Read(void* object, std::size_t offset) {
    T result{};
    if (!object || !native::CopyMemoryChecked(&result, static_cast<char*>(object) + offset, sizeof(result)))
        throw std::runtime_error("actor timing field unreadable");
    return result;
}
struct TimingNative : detail::TimingBindings {
    std::shared_ptr<RuntimeMetadata> metadata;
    MethodResolver* resolver;
    ManagedObjects* objects;
    detail::ActorTimingFactory::Fields fields;
    std::function<bool(EntityToken, GameWorld*)> owns;
    EntityAccess entities;
    ModelAccess models;
    EntityOwner owner;
    EntityToken token;
    GameWorld* world;
    ManagedRoots roots;
    std::map<void*, ManagedRootId> handles;
    ReferenceMethods references;
    std::shared_ptr<TimingNative> provider;
    std::map<std::string, void*> texts;
    bool closed = false;
    struct Bound {
        ActorTimingModel identity;
        void* component = nullptr;
        void* animator = nullptr;
        bool battle = false;
    };
    struct Clock {
        void* stack = nullptr;
        int index = 0;
        void* key = nullptr;
    };
    std::map<std::uint64_t, Clock> clocks;
    std::uint64_t next = 0;
    TimingNative(MethodResolver& methods, ManagedObjects& managed, detail::ActorTimingFactory::Fields layout,
                 ReferenceMethods refs, EntityAccess e, ModelAccess model, EntityOwner o, EntityToken t, GameWorld* w)
        : resolver(&methods), objects(&managed), fields(std::move(layout)), entities(std::move(e)),
          models(std::move(model)), owner(std::move(o)), token(t), world(w), roots(refs), references(refs) {}
    TimingNative(std::shared_ptr<TimingNative> p, EntityToken t, GameWorld* w)
        : TimingNative(*p->resolver, *p->objects, p->fields, p->references, p->entities, p->models, p->owner, t, w) {
        provider = std::move(p);
        const auto& b = *provider;
        metadata = b.metadata;
        owns = b.owns;
        static_cast<detail::TimingBindings&>(*this) = b;
    }
    void RetainOwner() {
        Keep(token.entity);
        Keep(world);
    }
    void* Keep(void* object) {
        if (!object)
            return nullptr;
        if (!handles.count(object)) {
            const auto id = roots.Keep(object);
            handles[object] = id;
            if (!roots.Status(id).ready)
                throw std::runtime_error("actor timing retention failed");
        }
        return object;
    }
    bool Live() const {
        if (closed || !world || !token.entity || !token.generation ||
            (owns ? !owns(token, world) : owner.OwnedToken(token.entity).generation != token.generation) ||
            TrackedEntities().IsDisposed(token) || TrackedEntities().IsRetiring(token))
            return false;
        const auto current = entities.Read(token.entity);
        if (!current.owner_world.read.returned)
            throw std::runtime_error("actor timing owner world unreadable");
        return current.owner_world.value == world && !TrackedEntities().IsDisposed(token);
    }
    void* Type(const char* ns, const char* name) {
        const auto result = objects->ManagedType(ns, name);
        if (!result.call.returned || !result.object)
            throw std::runtime_error(std::string("actor timing type absent: ") + name);
        return Keep(result.object);
    }
    void* Text(const std::string& value) {
        if (const auto found = texts.find(value); found != texts.end())
            return found->second;
        const auto result = objects->NewStringUtf8(value.c_str());
        if (!result.call.returned || !result.object)
            throw std::runtime_error("actor timing string allocation failed");
        texts.emplace(value, Keep(result.object));
        return result.object;
    }
    std::optional<Bound> Bind() {
        if (!Live())
            return {};
        if (!animator_error.empty())
            throw std::runtime_error(animator_error);
        const auto model = entities.Component(token.entity, model_type);
        if (!model.call.returned)
            throw std::runtime_error("actor timing model component lookup failed");
        if (!model.object)
            return {};
        Keep(model.object);
        const auto current = models.Read(static_cast<CharacterModelComponent*>(model.object));
        if (!current.changing.read.returned || !current.art.read.returned || !current.animator.read.returned)
            throw std::runtime_error("actor timing model observation failed");
        if (current.changing.value || !current.art.value || !current.animator.value)
            return {};
        Keep(current.art.value);
        Keep(current.animator.value);
        ObjectResult component{{true, 0}, nullptr};
        void* actual = nullptr;
        bool battle = false;
        if (battle_error.empty())
            component = entities.Component(token.entity, battle_type);
        if (!component.call.returned)
            throw std::runtime_error("actor timing battle component lookup failed");
        if (component.object) {
            Keep(component.object);
            if (!native::Call(battle_animator, &actual, component.object).returned)
                throw std::runtime_error("actor timing battle Animator getter failed");
            battle = actual == current.animator.value;
        }
        if (!battle) {
            if (!adventure_error.empty())
                throw std::runtime_error(adventure_error);
            component = entities.Component(token.entity, adventure_type);
            if (!component.call.returned)
                throw std::runtime_error("actor timing exploration component lookup failed");
            if (!component.object)
                return {};
            Keep(component.object);
            void* mock = nullptr;
            if (!native::Call(adventure_mock, &mock, component.object).returned)
                throw std::runtime_error("actor timing exploration MockAnimator getter failed");
            if (!mock)
                return {};
            Keep(mock);
            if (!native::Call(mock_animator, &actual, mock).returned)
                throw std::runtime_error("actor timing exploration Animator getter failed");
        }
        if (actual != current.animator.value || !Live())
            return {};
        void* controller_object = nullptr;
        if (!native::Call(controller, &controller_object, actual).returned)
            throw std::runtime_error("actor timing controller getter failed");
        Keep(controller_object);
        return Bound{{reinterpret_cast<std::uintptr_t>(model.object), reinterpret_cast<std::uintptr_t>(actual),
                      reinterpret_cast<std::uintptr_t>(component.object),
                      reinterpret_cast<std::uintptr_t>(current.art.value),
                      reinterpret_cast<std::uintptr_t>(controller_object)},
                     component.object,
                     actual,
                     battle};
    }
    bool Current(const Bound& expected) {
        const auto now = Bind();
        return now && now->identity == expected.identity;
    }
    void* Stack() {
        if (!clock_error.empty())
            throw std::runtime_error(clock_error);
        return Live() ? Keep(Read<void*>(token.entity, clock_offset)) : nullptr;
    }
    bool Has(const Clock& clock) {
        if (Stack() != clock.stack)
            return false;
        bool value = false;
        if (!native::Call(contains, &value, clock.stack, clock.index).returned)
            throw std::runtime_error("actor clock index unread");
        return value;
    }
    bool Remove(std::uint64_t id, bool fade) {
        const auto found = clocks.find(id);
        if (found == clocks.end())
            return true;
        const auto retire = [&] {
            const auto root = handles.find(found->second.key);
            if (root != handles.end()) {
                if (!roots.Release(root->second).complete)
                    return false;
                roots.Forget(root->second);
                handles.erase(root);
            }
            clocks.erase(found);
            return true;
        };
        if (!Has(found->second))
            return retire();
        if (!native::CallVoid(remove, found->second.stack, found->second.index, fade).returned)
            return false;
        if (!Has(found->second))
            return retire();
        return fade;
    }
    bool Close() {
        bool complete = true;
        std::vector<std::uint64_t> ids;
        for (const auto& [id, clock] : clocks) {
            (void)clock;
            ids.push_back(id);
        }
        for (const auto id : ids)
            if (!Remove(id, false))
                complete = false;
        if (!complete)
            return false;
        closed = true;
        for (auto it = handles.begin(); it != handles.end();) {
            if (roots.Release(it->second).complete) {
                roots.Forget(it->second);
                it = handles.erase(it);
            } else {
                complete = false;
                ++it;
            }
        }
        return complete;
    }
};
void Prepare(const std::shared_ptr<TimingNative>& state) {
    static_cast<detail::TimingBindings&>(*state) = detail::ResolveTimingBindings(
        *state->resolver, state->fields, [&](const char* ns, const char* name) { return state->Type(ns, name); });
}
ActorTimingServices Calls(const std::shared_ptr<TimingNative>& state) {
    ActorTimingServices calls;
    calls.layer = [state](const std::string& layer) -> std::optional<ActorTimingValue> {
        if (!state->layer_error.empty())
            throw std::runtime_error(state->layer_error);
        const auto bound = state->Bind();
        if (!bound)
            return {};
        int index = -1;
        float value = 0;
        auto* name = state->Text(layer);
        if (!native::Call(state->layer_index, &index, bound->animator, name).returned)
            throw std::runtime_error("actor timing layer index getter failed");
        if (index < 0)
            return {};
        if (!native::Call(state->layer_weight, &value, bound->animator, index).returned)
            throw std::runtime_error("actor timing layer weight getter failed");
        if (!state->Current(*bound))
            return {};
        return ActorTimingValue{bound->identity, value};
    };
    calls.set_layer = [state](const ActorTimingModel& expected, const std::string& layer, float weight,
                              float duration) {
        const auto bound = state->Bind();
        if (!bound || !(bound->identity == expected))
            return false;
        const auto& error = bound->battle ? state->battle_tween_error : state->adventure_tween_error;
        if (!error.empty())
            throw std::runtime_error(error);
        auto* name = state->Text(layer);
        return state->Current(*bound) &&
               detail::WriteTimingLayer(bound->battle ? state->battle_tween : state->adventure_tween, bound->component,
                                        name, weight, duration)
                   .returned;
    };
    calls.speed = [state]() -> std::optional<ActorTimingValue> {
        if (!state->speed_error.empty())
            throw std::runtime_error(state->speed_error);
        const auto bound = state->Bind();
        return bound && bound->battle
                   ? std::optional<ActorTimingValue>{{bound->identity,
                                                      Read<float>(bound->component, state->speed_offset)}}
                   : std::nullopt;
    };
    calls.set_speed = [state](const ActorTimingModel& expected, float value) {
        if (!state->speed_error.empty())
            throw std::runtime_error(state->speed_error);
        const auto bound = state->Bind();
        return bound && bound->battle && bound->identity == expected &&
               native::CopyMemoryChecked(static_cast<char*>(bound->component) + state->speed_offset, &value,
                                         sizeof(value));
    };
    calls.scale = [state](const ActorTimeScaleInput& input, const std::string& key) {
        const auto& error = input.duration ? state->finite_error : state->infinite_error;
        if (!error.empty())
            throw std::runtime_error(error);
        auto* stack = state->Stack();
        if (!stack)
            return ActorTimeScaleToken{};
        const auto text = state->objects->NewStringUtf8(key.c_str());
        if (!text.call.returned || !text.object)
            throw std::runtime_error("actor time scale key allocation failed");
        auto* name = state->Keep(text.object);
        int index = -1;
        const auto call =
            detail::StartActorClock(state->scale_finite, state->scale_infinite, stack, name, input, &index);
        if (!call.returned || index < 0)
            return ActorTimeScaleToken{};
        const auto id = ++state->next;
        state->clocks.emplace(id, TimingNative::Clock{stack, index, name});
        return ActorTimeScaleToken{id};
    };
    calls.remove = [state](ActorTimeScaleToken token, bool fade) { return state->Remove(token.value, fade); };
    calls.contains = [state](ActorTimeScaleToken token) -> std::optional<bool> {
        const auto found = state->clocks.find(token.value);
        return found != state->clocks.end() && state->Has(found->second);
    };
    calls.close = [state] { return state->Close(); };
    return calls;
}
}
namespace detail {
struct ActorTimingFactory::State {
    std::shared_ptr<TimingNative> bindings;
    std::vector<std::weak_ptr<TimingNative>> clients;
    bool closed = false;
};
ActorTimingFactory::ActorTimingFactory(RuntimeMetadata& m, ReferenceMethods refs, EntityAccess entities,
                                       ModelAccess models, EntityOwner owner)
    : ActorTimingFactory(m, m, [&m](const char* ns, const char* name) { return m.ReadFields(ns, name, false); }, refs,
                         std::move(entities), std::move(models), std::move(owner), {}) {

    state_->bindings->metadata = RetainRuntimeMetadata(m);
}
ActorTimingFactory::ActorTimingFactory(MethodResolver& resolver, ManagedObjects& managed, Fields fields,
                                       ReferenceMethods refs, EntityAccess entities, ModelAccess models,
                                       EntityOwner owner, std::function<bool(EntityToken, GameWorld*)> owns)
    : state_(std::make_unique<State>()) {
    state_->bindings = std::make_shared<TimingNative>(resolver, managed, std::move(fields), refs, std::move(entities),
                                                      std::move(models), std::move(owner), EntityToken{}, nullptr);
    state_->bindings->owns = std::move(owns);
    Prepare(state_->bindings);
}
ActorTimingFactory::~ActorTimingFactory() = default;
std::unique_ptr<ActorTiming> ActorTimingFactory::Create(EntityToken token, GameWorld* world) {
    if (state_->closed)
        throw std::logic_error("actor timing factory closed");
    state_->clients.erase(
        std::remove_if(state_->clients.begin(), state_->clients.end(), [](const auto& weak) { return weak.expired(); }),
        state_->clients.end());
    auto client = std::make_shared<TimingNative>(state_->bindings, token, world);
    try {
        client->RetainOwner();
        auto result = std::make_unique<ActorTiming>(Calls(client));
        state_->clients.push_back(client);
        return result;
    } catch (const std::exception& error) {
        if (!client->Close())
            throw std::runtime_error(std::string(error.what()) + "; actor timing creation input cleanup unresolved");
        throw;
    }
}
bool ActorTimingFactory::Close() {
    state_->closed = true;
    bool complete = true;
    for (const auto& weak : state_->clients)
        if (const auto client = weak.lock())
            if (!client->Close())
                complete = false;
    if (complete)
        complete = state_->bindings->Close();
    return complete;
}
}
}
