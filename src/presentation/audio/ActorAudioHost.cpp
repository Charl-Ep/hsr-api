#include "ActorAudioHost.hpp"
#include <hksr/model/ModelAccess.hpp>
#include "SoundManager.hpp"
#include "VoiceResources.hpp"
#include "ActorAudioSource.hpp"
#include "AudioPostRoute.hpp"
#include "../../../adapters/presentation/AudioPostBindings.hpp"
#include "../../../adapters/presentation/ContextBindings.hpp"
#include <hksr/animation/AnimationResources.hpp>
#include <hksr/presentation/audio/OwnedSoundRequest.hpp>
#include <hksr/presentation/events/AnimationEvents.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <map>
#include <unordered_map>
#include <stdexcept>

namespace hksr::api::detail {
namespace {
bool Alive(EntityToken token) {
    return token.entity && !TrackedEntities().IsDisposed(token) &&
           TrackedEntities().Current(token.entity).generation == token.generation;
}
}
struct ActorAudioHost::State {
    ManagedObjects& objects;
    ReferenceMethods references;
    EntityAccess entities;
    ModelAccess models;
    EntityOwner owner;
    ManagedArrays arrays;
    ComponentConfig components;
    ManagedRoots roots;
    RuntimeMetadata* metadata = nullptr;
    std::function<void(const std::string&)> trace;
    std::vector<ManagedRootId> type_roots;
    void* event_type = nullptr;
    void* model_type = nullptr;
    std::unique_ptr<detail::SoundManager> sounds;
    std::unique_ptr<detail::AudioPostRoute> audio_route;
    struct AudioActor {
        EntityOrigin origin;
        std::unordered_map<std::string, ActorAudioEvent> events;
        unsigned rate = 1;
        detail::ActorAudioSource source;
        void* indexed_emitter = nullptr;
    };
    std::map<GameEntity*, AudioActor> audio_actors;
    std::unordered_map<void*, GameEntity*> audio_emitters;
    std::vector<ManagedRootId> audio_releases;
    void* audio_manager = nullptr;
    ManagedRootId audio_manager_root = 0;
    std::function<void*()> audio_callback;
    bool closed = false;
    std::optional<detail::VoiceResourceAccess> voice_resources;

    State(ManagedObjects& o, ReferenceMethods refs, EntityAccess e, ModelAccess m, EntityOwner own, ManagedArrays a,
          ComponentConfig c)
        : objects(o), references(refs), entities(std::move(e)), models(std::move(m)), owner(std::move(own)),
          arrays(std::move(a)), components(std::move(c)), roots(refs) {}
    void* ComponentType(void*& cached, detail::ActorComponent component) {
        if (!cached) {
            const auto type = detail::ResolveActorComponentType(objects, component);
            if (!type.call.returned || !type.object)
                throw std::runtime_error("actor component Type unavailable");
            type_roots.push_back(0);
            type_roots.back() = roots.Keep(type.object);
            const auto kept = roots.Target(type_roots.back());
            if (!kept.call.returned || !kept.object)
                throw std::runtime_error("actor component Type retention failed");
            cached = kept.object;
        }
        return cached;
    }
    void* EventType() {
        if (!event_type) {
            const auto type = ResolveAnimationInputTypes(objects).events;
            if (!type.call.returned || !type.object)
                throw std::runtime_error("event Type unavailable");
            type_roots.push_back(0);
            type_roots.back() = roots.Keep(type.object);
            const auto kept = roots.Target(type_roots.back());
            if (!kept.call.returned || !kept.object)
                throw std::runtime_error("event Type retention failed");
            event_type = kept.object;
        }
        return event_type;
    }
    void Log(const std::string& text) const noexcept {
        if (trace)
            try {
                trace(text);
            } catch (...) {
            }
    }
    void* AudioEmitter(EntityToken token, GameWorld* world) {
        if (!world || closed || !Alive(token) || owner.OwnedToken(token.entity).generation != token.generation)
            return nullptr;
        const auto entity = entities.Read(token.entity);
        if (!entity.owner_world.read.returned || entity.owner_world.value != world || !Alive(token))
            return nullptr;
        const auto events = entities.Component(token.entity, EventType());
        if (events.call.returned && events.object) {
            auto* art = detail::BindAnimationAudioEmitter(*metadata, events.object)();
            if (art && Alive(token))
                return art;
        }
        const auto model = entities.Component(token.entity, ComponentType(model_type, detail::ActorComponent::Model));
        if (!model.call.returned || !model.object || !Alive(token))
            return nullptr;
        const auto art = models.Art(static_cast<CharacterModelComponent*>(model.object));
        return art.read.returned && Alive(token) ? art.value : nullptr;
    }
    void RefreshAudioManager() {
        if (!sounds)
            sounds = std::make_unique<detail::SoundManager>(*metadata, references);
        const auto manager = sounds->Manager();
        if (!manager.call.returned || !manager.object)
            throw std::runtime_error("audio manager unavailable");
        if (manager.object == audio_manager)
            return;
        auto peek = detail::BindAudioManagerCallback(*metadata, manager.object);
        audio_releases.reserve(audio_releases.size() + 2);
        const auto key = roots.Keep(manager.object);
        if (!roots.Status(key).ready) {
            audio_releases.push_back(key);
            throw std::runtime_error("audio manager retention failed");
        }
        if (audio_manager_root)
            audio_releases.push_back(audio_manager_root);
        audio_manager = manager.object;
        audio_manager_root = key;
        audio_callback = std::move(peek);
    }
    detail::VoiceResourceAccess& Voices() {
        if (!metadata || closed)
            throw std::logic_error("voice resources unavailable");
        if (!voice_resources)
            voice_resources =
                detail::ResolveVoiceResources(*metadata, *metadata, objects, references, arrays, components);
        return *voice_resources;
    }
    void UnindexAudioActor(AudioActor& actor) {
        if (actor.indexed_emitter) {
            const auto old = audio_emitters.find(actor.indexed_emitter);
            if (old != audio_emitters.end() && old->second == actor.origin.token.entity)
                audio_emitters.erase(old);
        }
        actor.indexed_emitter = nullptr;
    }
    void RefreshAudioActor(AudioActor& actor) {
        if (!Alive(actor.origin.token) ||
            owner.OwnedToken(actor.origin.token.entity).generation != actor.origin.token.generation)
            return;
        const auto component = entities.Component(actor.origin.token.entity, EventType());
        if (!component.call.returned) {
            ReleaseAudioActor(actor);
            throw std::runtime_error("current audio source unavailable");
        }
        actor.source.Refresh(
            component.object, roots, audio_releases,
            [this](void* current) { return detail::BindAnimationAudioEmitter(*metadata, current); },
            [this, &actor] { UnindexAudioActor(actor); });
        auto* emitter = actor.source.Emitter();
        if (emitter == actor.indexed_emitter)
            return;
        UnindexAudioActor(actor);
        if (emitter) {
            auto inserted = audio_emitters.emplace(emitter, actor.origin.token.entity);
            if (!inserted.second && inserted.first->second != actor.origin.token.entity) {
                const auto previous = audio_actors.find(inserted.first->second);
                if (previous == audio_actors.end())
                    audio_emitters.erase(emitter);
                else if (!Alive(previous->second.origin.token) ||
                         TrackedEntities().IsRetiring(previous->second.origin.token) ||
                         owner.OwnedToken(previous->first).generation != previous->second.origin.token.generation)
                    ReleaseAudioActor(previous->second);
                else
                    RefreshAudioActor(previous->second);
                inserted = audio_emitters.emplace(emitter, actor.origin.token.entity);
                if (!inserted.second && inserted.first->second != actor.origin.token.entity)
                    throw std::runtime_error("owned audio emitter shared by different actors");
            }
            actor.indexed_emitter = emitter;
        }
    }
    void RefreshAudioActor(GameEntity* entity) {
        const auto found = audio_actors.find(entity);
        if (found != audio_actors.end())
            RefreshAudioActor(found->second);
    }
    void ReleaseAudioActor(AudioActor& actor) {
        actor.source.Reset(audio_releases, [this, &actor] { UnindexAudioActor(actor); });
    }
    void CollectAudio() {
        if (audio_route)
            audio_route->Collect();
        for (auto it = audio_actors.begin(); it != audio_actors.end();) {
            if (owner.OwnedToken(it->first).generation != it->second.origin.token.generation ||
                !Alive(it->second.origin.token)) {
                ReleaseAudioActor(it->second);
                it = audio_actors.erase(it);
            } else {
                try {
                    RefreshAudioActor(it->second);
                } catch (const std::exception& e) {
                    Log(e.what());
                }
                ++it;
            }
        }
        for (auto it = audio_releases.begin(); it != audio_releases.end();)
            if (roots.Release(*it).complete) {
                roots.Forget(*it);
                it = audio_releases.erase(it);
            } else
                ++it;
    }
    void RefreshAudioEmitters() {
        if (closed)
            return;
        if (!audio_actors.empty())
            try {
                RefreshAudioManager();
            } catch (const std::exception& e) {
                Log(e.what());
            }
        for (auto& row : audio_actors)
            try {
                RefreshAudioActor(row.second);
            } catch (const std::exception& e) {
                Log(e.what());
            }
    }
    std::optional<std::uint32_t> SelectAudio(const detail::AudioPostArguments& args) {
        if (closed || !args.event_name || !args.emitter)
            return {};
        if (!audio_callback || !args.callback || audio_callback() != args.callback)
            return {};
        // Unrelated emitters incur no entity/component reads. For a hit, verify
        // the current component identity before using its prepared field reader.
        const auto indexed = audio_emitters.find(args.emitter);
        if (indexed == audio_emitters.end())
            return {};
        const auto found = audio_actors.find(indexed->second);
        if (found == audio_actors.end())
            return {};
        const auto entity = found->first;
        auto& row = found->second;
        {
            const auto token = row.origin.token;
            if (!Alive(token) || TrackedEntities().IsRetiring(token) ||
                owner.OwnedToken(entity).generation != token.generation)
                return {};
            const auto current = entities.Read(entity);
            if (!current.owner_world.read.returned || current.owner_world.value != row.origin.world)
                return {};
            RefreshAudioActor(row);
            if (row.source.Emitter() != args.emitter)
                return {};
            const auto text = components.ReadText(args.event_name);
            if (!text.read.returned)
                return {};
            const auto event = row.events.find(text.utf8);
            if (event != row.events.end())
                return row.rate == 2 ? event->second.doubled_node : event->second.normal_node;
        }
        return {};
    }
};
ActorAudioHost::ActorAudioHost(ManagedObjects& o, ReferenceMethods refs, EntityAccess e, ModelAccess m,
                               EntityOwner owner, ManagedArrays a, ComponentConfig c)
    : state_(
          std::make_shared<State>(o, refs, std::move(e), std::move(m), std::move(owner), std::move(a), std::move(c))) {}
ActorAudioHost::~ActorAudioHost() = default;
void ActorAudioHost::Prepare(RuntimeMetadata& metadata, std::function<void(const std::string&)> trace) {
    state_->metadata = &metadata;
    state_->trace = std::move(trace);
}
std::unique_ptr<OwnedSoundRequest> ActorAudioHost::Start(EntityToken token, GameWorld* world,
                                                         const std::string& event) const {
    const auto s = state_;
    if (!s->metadata || s->closed)
        throw std::logic_error("audio context unavailable");
    if (event.empty() || event.find('\0') != std::string::npos)
        throw std::invalid_argument("audio event name is empty or contains NUL");
    const auto source = [s, token, world] { return s->AudioEmitter(token, world); };
    s->RefreshAudioActor(token.entity);
    auto* emitter = source();
    if (!emitter)
        throw std::runtime_error("audio requires this client's current owned model");
    if (!s->sounds)
        s->sounds = std::make_unique<detail::SoundManager>(*s->metadata, s->references);
    auto services = s->sounds->Services();
    services.current = [source, emitter] { return source() == emitter; };
    const auto manager = s->sounds->Manager();
    if (!manager.call.returned || !manager.object)
        throw std::runtime_error("audio manager unavailable");
    const auto name = s->objects.NewStringUtf8(event.c_str());
    if (!name.call.returned || !name.object)
        throw std::runtime_error("audio name allocation failed");
    auto request = std::make_unique<OwnedSoundRequest>(std::move(services));
    request->Start(manager.object, name.object, emitter);
    return request;
}
void ActorAudioHost::Set(EntityToken token, GameWorld* world, std::vector<ActorAudioEvent> events, unsigned rate) {
    auto& s = *state_;
    if (events.empty()) {
        Clear(token, world);
        return;
    }
    if (rate != 1 && rate != 2)
        throw std::invalid_argument("audio rate must be 1 or 2");
    if (!s.metadata || s.closed)
        throw std::logic_error("audio route unavailable");
    if (!world || !Alive(token) || s.owner.OwnedToken(token.entity).generation != token.generation)
        throw std::runtime_error("audio mapping requires this client's owned actor");
    const auto actor = s.entities.Read(token.entity);
    if (!actor.owner_world.read.returned || actor.owner_world.value != world)
        throw std::runtime_error("audio mapping world differs");
    std::unordered_map<std::string, ActorAudioEvent> selected;
    selected.reserve(events.size());
    for (const auto& row : events) {
        if (row.event.empty() || row.event.find('\0') != std::string::npos || !row.normal_node || !row.doubled_node ||
            !selected.emplace(row.event, row).second)
            throw std::invalid_argument("audio mapping requires unique events and nonzero nodes");
    }
    s.RefreshAudioManager();
    if (!s.audio_route) {
        const auto weak = std::weak_ptr<State>(state_);
        s.audio_route = std::make_unique<detail::AudioPostRoute>(
            detail::ResolveAudioPostOperations(*s.metadata, s.references),
            [weak](const auto& args) -> std::optional<std::uint32_t> {
                const auto state = weak.lock();
                return state ? state->SelectAudio(args) : std::nullopt;
            },
            [weak](const std::string& message) {
                if (const auto state = weak.lock())
                    state->Log("audio route: " + message);
            });
    }
    auto [found, inserted] = s.audio_actors.try_emplace(token.entity);
    (void)inserted;
    auto& entry = found->second;
    try {
        if (entry.origin.token.generation != token.generation || entry.origin.world != world) {
            s.ReleaseAudioActor(entry);
            entry.origin = {token, world};
        }
        entry.events = std::move(selected);
        entry.rate = rate;
        s.RefreshAudioActor(entry);
    } catch (...) {
        s.ReleaseAudioActor(entry);
        s.audio_actors.erase(found);
        throw;
    }
}
void ActorAudioHost::SetRate(EntityToken token, GameWorld* world, unsigned rate) {
    auto& s = *state_;
    if (rate != 1 && rate != 2)
        throw std::invalid_argument("audio rate must be 1 or 2");
    if (s.closed)
        throw std::logic_error("audio route closed");
    const auto found = s.audio_actors.find(token.entity);
    if (found == s.audio_actors.end() || found->second.origin.token.generation != token.generation ||
        found->second.origin.world != world)
        throw std::runtime_error("actor audio mapping is not registered");
    found->second.rate = rate;
}
void ActorAudioHost::Clear(EntityToken token, GameWorld* world) {
    auto& s = *state_;
    const auto found = s.audio_actors.find(token.entity);
    if (found != s.audio_actors.end() && found->second.origin.token.generation == token.generation &&
        found->second.origin.world == world) {
        s.ReleaseAudioActor(found->second);
        s.audio_actors.erase(found);
    }
}
std::string ActorAudioHost::ReadVoiceEvent(EntityToken token, GameWorld* world, std::uint32_t avatar,
                                           VoiceCue cue) const {
    auto& s = *state_;
    if (!world || !Alive(token) || s.owner.OwnedToken(token.entity).generation != token.generation)
        throw std::invalid_argument("voice resources require this client's owned actor");
    const auto current = s.entities.Read(token.entity);
    if (!current.owner_world.read.returned || current.owner_world.value != world)
        throw std::runtime_error("voice resource world differs");
    return s.Voices().read(avatar, cue);
}
std::optional<VoiceCue> ActorAudioHost::ReadReadyVoiceCue(std::int32_t type) const {
    return state_->Voices().ready(type);
}

bool ActorAudioHost::VoiceExists(const std::string& event) const {
    return state_->Voices().exists(event);
}
void ActorAudioHost::Refresh(GameEntity* entity) {
    state_->RefreshAudioActor(entity);
}
void ActorAudioHost::Activate() {
    state_->RefreshAudioEmitters();
}
void ActorAudioHost::Collect() {
    state_->CollectAudio();
}
void ActorAudioHost::BeginClose() {
    state_->closed = true;
    if (state_->audio_route)
        state_->audio_route->Stop();
}
bool ActorAudioHost::Close() {
    BeginClose();
    auto& s = *state_;
    for (auto& row : s.audio_actors)
        s.ReleaseAudioActor(row.second);
    s.audio_actors.clear();
    s.audio_emitters.clear();
    if (s.audio_manager_root) {
        s.audio_releases.push_back(s.audio_manager_root);
        s.audio_manager_root = 0;
    }
    s.audio_manager = nullptr;
    s.audio_callback = {};
    s.CollectAudio();
    bool closed = !s.voice_resources || s.voice_resources->close();
    if ((s.audio_route && !s.audio_route->Collect()) || !s.audio_releases.empty())
        closed = false;
    if (s.sounds && !s.sounds->Close())
        closed = false;
    for (auto it = s.type_roots.begin(); it != s.type_roots.end();)
        if (s.roots.Release(*it).complete) {
            s.roots.Forget(*it);
            it = s.type_roots.erase(it);
        } else {
            closed = false;
            ++it;
        }
    return closed;
}
}
