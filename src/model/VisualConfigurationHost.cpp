#include "VisualConfigurationHost.hpp"
#include "../../adapters/model/ModelBindings.hpp"
#include "../../adapters/model/VisualConfigurationBindings.hpp"
#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <stdexcept>

namespace hksr::api::detail {
struct VisualConfigurationHost::State {
    ManagedObjects& objects;
    EntityAccess entities;
    std::function<bool(EntityToken)> owns;
    AnimationEvents events;
    ActorVisualConfigurationFactory factory;
    ManagedRoots roots;
    std::vector<ManagedRootId> held;
    void *model_type = nullptr, *data_type = nullptr, *emotion_type = nullptr, *event_type = nullptr;
    bool closed = false;

    State(MethodResolver& methods, ObjectMetadata& metadata, ManagedObjects& o,
          ActorVisualConfigurationFactory::Fields fields, ReferenceMethods refs, ManagedArrays arrays, EntityAccess e,
          std::function<bool(EntityToken)> own, AnimationEvents ev)
        : objects(o), entities(std::move(e)), owns(std::move(own)), events(std::move(ev)),
          factory(methods, metadata, o, refs, std::move(arrays), std::move(fields)), roots(refs) {}
    bool Owned(EntityToken token) const {
        return !closed && token.entity && token.generation && !TrackedEntities().IsDisposed(token) &&
               !TrackedEntities().IsRetiring(token) &&
               TrackedEntities().Current(token.entity).generation == token.generation && owns && owns(token);
    }
    void* Keep(ObjectResult type) {
        if (!type.call.returned || !type.object)
            throw std::runtime_error("visual configuration Type unavailable");
        const auto id = roots.Keep(type.object);
        held.push_back(id);
        const auto kept = roots.Target(id);
        if (!kept.call.returned || !kept.object)
            throw std::runtime_error("visual configuration Type retention failed");
        return kept.object;
    }
    void* Type(void*& cached, VisualConfigurationObject kind) {
        if (!cached)
            cached = Keep(ResolveVisualConfigurationObjectType(objects, kind));
        return cached;
    }
    bool Close() {
        closed = true;
        if (!factory.Close())
            return false;
        for (auto it = held.begin(); it != held.end();) {
            if (roots.Release(*it).complete) {
                roots.Forget(*it);
                it = held.erase(it);
            } else
                ++it;
        }
        return held.empty();
    }
};
VisualConfigurationHost::VisualConfigurationHost(MethodResolver& methods, ObjectMetadata& metadata,
                                                 ManagedObjects& objects,
                                                 ActorVisualConfigurationFactory::Fields fields, ReferenceMethods refs,
                                                 ManagedArrays arrays, EntityAccess entities,
                                                 std::function<bool(EntityToken)> owns, AnimationEvents events)
    : state_(std::make_shared<State>(methods, metadata, objects, std::move(fields), refs, std::move(arrays),
                                     std::move(entities), std::move(owns), std::move(events))) {}
VisualConfigurationHost::~VisualConfigurationHost() = default;
std::unique_ptr<ActorVisualConfiguration> VisualConfigurationHost::Create(EntityToken token, GameWorld* world) {
    const auto s = state_;
    if (!world || !s->Owned(token))
        throw std::invalid_argument("visual configuration requires this client's owned actor");
    const auto entity = s->entities.Read(token.entity);
    if (!entity.owner_world.read.returned || entity.owner_world.value != world)
        return {};
    if (!s->model_type)
        s->model_type = s->Keep(ResolveCharacterModelType(s->objects));
    const auto model = s->entities.Component(token.entity, s->model_type);
    if (!model.call.returned || !model.object)
        return {};
    struct Component {
        void* object = nullptr;
        std::string error;
    } data, emotion, event;
    void* receiver = nullptr;
    auto capture = [&](Component& into, auto type) {
        try {
            const auto value = s->entities.Component(token.entity, type());
            if (!value.call.returned)
                throw std::runtime_error("visual configuration component lookup failed");
            into.object = value.object;
        } catch (const std::exception& failure) {
            into.error = failure.what();
        }
    };
    capture(data, [&] { return s->Type(s->data_type, VisualConfigurationObject::Data); });
    capture(emotion, [&] { return s->Type(s->emotion_type, VisualConfigurationObject::Emotion); });
    capture(event, [&] { return s->Type(s->event_type, VisualConfigurationObject::EventComponent); });
    if (event.error.empty() && event.object)
        try {
            const auto consumer = s->events.Consumer(event.object);
            if (!consumer.call.returned)
                throw std::runtime_error("visual configuration event consumer unreadable");
            receiver = consumer.object;
        } catch (const std::exception& failure) {
            event.error = failure.what();
        }
    auto current = [s, token, world, model = model.object] {
        if (!s->Owned(token))
            return false;
        const auto entity = s->entities.Read(token.entity);
        if (!entity.owner_world.read.returned || entity.owner_world.value != world)
            return false;
        const auto m = s->entities.Component(token.entity, s->model_type);
        return m.call.returned && m.object == model && s->Owned(token);
    };
    auto current_component = [s, token, data, emotion, event, receiver](VisualConfigurationComponent use) {
        const auto& captured = use == VisualConfigurationComponent::Data      ? data
                               : use == VisualConfigurationComponent::Emotion ? emotion
                                                                             : event;
        if (!captured.error.empty())
            throw std::runtime_error(captured.error);
        auto* type = use == VisualConfigurationComponent::Data      ? s->data_type
                     : use == VisualConfigurationComponent::Emotion ? s->emotion_type
                                                                   : s->event_type;
        const auto actual = s->entities.Component(token.entity, type);
        if (!actual.call.returned)
            throw std::runtime_error("visual configuration current component unreadable");
        if (actual.object != captured.object)
            return false;
        if (use == VisualConfigurationComponent::Events && event.object) {
            const auto r = s->events.Consumer(event.object);
            if (!r.call.returned || r.object != receiver)
                return false;
        }
        return s->Owned(token);
    };
    if (!current())
        return {};
    return s->factory.Bind({{token, world},
                            reinterpret_cast<std::uintptr_t>(model.object),
                            data.object,
                            emotion.object,
                            std::move(current),
                            receiver,
                            event.object != nullptr,
                            std::move(current_component)});
}
bool VisualConfigurationHost::Close() {
    return state_->Close();
}
}
