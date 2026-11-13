#include "ActorSkinBindings.hpp"
#include "CharacterSkinNative.hpp"
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <map>
#include <algorithm>
#include <stdexcept>

namespace hksr::api::detail {
struct ActorSkinBindings::State {
    ManagedObjects& objects;
    ReferenceMethods references;
    EntityOwner owner;
    ManagedArrays arrays;
    ComponentConfig components;
    RuntimeMetadata* metadata = nullptr;
    bool closing = false;
    struct Actor {
        EntityOrigin origin;
        std::shared_ptr<CharacterSkin> skin;
    };
    std::map<GameEntity*, Actor> actors;
    std::vector<std::shared_ptr<CharacterSkin>> skins;
    std::vector<std::function<bool()>> reads;
    State(ManagedObjects& o, ReferenceMethods refs, EntityOwner own, ManagedArrays a, ComponentConfig c)
        : objects(o), references(refs), owner(std::move(own)), arrays(std::move(a)), components(std::move(c)) {}
    void Retain(const std::shared_ptr<CharacterSkin>& skin) {
        if (!skin)
            return;
        if (closing || !skin->Status().prepared || skin->Status().closed)
            throw std::invalid_argument("entity skin is not prepared or its owner is closing");
        if (std::find(skins.begin(), skins.end(), skin) == skins.end())
            skins.push_back(skin);
    }
    void Bind(EntityOrigin origin, const std::shared_ptr<CharacterSkin>& skin) {
        if (!skin->Status().prepared || skin->Status().closed)
            throw std::runtime_error("entity skin closed during construction");
        if (!origin.token.entity || !origin.token.generation || !origin.world ||
            owner.OwnedToken(origin.token.entity).generation != origin.token.generation)
            throw std::runtime_error("skin binding requires this client's exact birth receipt");
        const auto prior = actors.find(origin.token.entity);
        if (prior != actors.end() && prior->second.origin.token.generation == origin.token.generation &&
            prior->second.skin != skin)
            throw std::runtime_error("same actor birth has conflicting skin inputs");
        actors.insert_or_assign(origin.token.entity, Actor{origin, skin});
    }
    void Collect() {
        for (auto it = actors.begin(); it != actors.end();)
            if (owner.OwnedToken(it->first).generation != it->second.origin.token.generation)
                it = actors.erase(it);
            else
                ++it;
    }
};
ActorSkinBindings::ActorSkinBindings(ManagedObjects& o, ReferenceMethods refs, EntityOwner owner, ManagedArrays arrays,
                                     ComponentConfig components)
    : state_(std::make_shared<State>(o, refs, std::move(owner), std::move(arrays), std::move(components))) {}
ActorSkinBindings::~ActorSkinBindings() = default;
void ActorSkinBindings::Prepare(RuntimeMetadata& metadata) {
    state_->metadata = &metadata;
}
CharacterSkinCatalog ActorSkinBindings::Read() const {
    auto& s = *state_;
    if (!s.metadata || s.closing)
        throw std::logic_error("skin catalog requires an open prepared owner");
    std::function<bool()> cleanup;
    auto out =
        CharacterSkinFactory::Read(*s.metadata, *s.metadata, s.objects, s.references, s.arrays, s.components, cleanup);
    if (cleanup && !cleanup())
        s.reads.push_back(std::move(cleanup));
    return out;
}
std::shared_ptr<CharacterSkin> ActorSkinBindings::PrepareSkin(std::uint32_t id) const {
    auto& s = *state_;
    if (!s.metadata || s.closing)
        throw std::logic_error("skin preparation requires an open prepared owner");
    auto skin =
        CharacterSkinFactory::Prepare(*s.metadata, *s.metadata, s.objects, s.references, s.arrays, s.components, id);
    s.Retain(skin);
    return skin;
}
std::shared_ptr<CharacterSkin> ActorSkinBindings::Find(GameEntity* entity, GameWorld* world) const {
    const auto& s = *state_;
    const auto found = s.actors.find(entity);
    if (found == s.actors.end())
        return {};
    const auto& row = found->second;
    if (s.owner.OwnedToken(entity).generation != row.origin.token.generation || (world && world != row.origin.world))
        return {};
    return row.skin;
}
std::function<void(EntityOrigin)> ActorSkinBindings::BirthObserver(std::shared_ptr<CharacterSkin> skin,
                                                                   std::function<void(EntityOrigin)> remember) const {
    state_->Retain(skin);
    return [s = state_, skin = std::move(skin), remember = std::move(remember)](EntityOrigin origin) {
        if (skin)
            s->Bind(origin, skin);
        remember(origin);
    };
}
void ActorSkinBindings::Collect() {
    state_->Collect();
}
bool ActorSkinBindings::ActorsDrained() const {
    return state_->actors.empty();
}
bool ActorSkinBindings::Close() {
    auto& s = *state_;
    s.closing = true;
    s.Collect();
    if (!s.actors.empty())
        return false;
    bool closed = true;
    for (auto it = s.skins.begin(); it != s.skins.end();)
        if ((*it)->Close())
            it = s.skins.erase(it);
        else {
            closed = false;
            ++it;
        }
    for (auto it = s.reads.begin(); it != s.reads.end();)
        if ((*it)())
            it = s.reads.erase(it);
        else {
            closed = false;
            ++it;
        }
    return closed;
}
}
