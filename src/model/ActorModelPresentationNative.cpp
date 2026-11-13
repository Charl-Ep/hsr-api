#include "ActorModelPresentationNative.hpp"
#include <hksr/model/ModelAccess.hpp>
#include "../../adapters/model/ModelPresentationBindings.hpp"
#include "../../adapters/model/ModelPresentationCalls.hpp"
#include "ModelScaleNative.hpp"
#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/managed/GenericFactory.hpp>
#include <hksr/managed/ManagedLists.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include "../runtime/RuntimeResident.hpp"
#include <map>
#include <optional>

namespace hksr::api::detail {
namespace {
template <class T> T Read(void* object, std::size_t offset) {
    T value{};
    if (!object || !native::CopyMemoryChecked(&value, static_cast<char*>(object) + offset, sizeof(value)))
        throw std::runtime_error("model presentation field unreadable");
    return value;
}
}
struct ActorModelPresentationFactory::State : ModelPresentationBindings {
    struct Bound;
    std::vector<std::shared_ptr<Bound>> pending;
    std::shared_ptr<RuntimeMetadata> metadata;
    ReferenceMethods references;
    EntityAccess entities;
    ModelAccess models;
    EntityOwner owner;
    ManagedRoots roots;
    ManagedArrays arrays;
    std::unique_ptr<GenericFactory> follow_factory;
    std::function<void(EntityToken, GameWorld*)> ensure_face;
    std::vector<ManagedRootId> held;
    bool closed = false;
    std::shared_ptr<ModelScaleNative> scale_program;
    std::string scale_program_error;
    State(RuntimeMetadata& m, ReferenceMethods refs, EntityAccess e, ModelAccess models_, ManagedArrays arrays,
          EntityOwner o, std::function<void(EntityToken, GameWorld*)> face)
        : metadata(RetainRuntimeMetadata(m)), references(refs), entities(std::move(e)), models(std::move(models_)),
          owner(std::move(o)), roots(refs), arrays(std::move(arrays)), ensure_face(std::move(face)) {}
    void* Type(const char* space, const char* name) {
        const auto type = metadata->ManagedType(space, name);
        if (!type.call.returned || !type.object)
            throw std::runtime_error(std::string("model presentation Type unavailable: ") + name);
        held.push_back(roots.Keep(type.object));
        if (!roots.Status(held.back()).ready)
            throw std::runtime_error("model presentation Type retention failed");
        return type.object;
    }
    bool Owned(EntityToken token, GameWorld* world) const {
        if (closed || !world || !token.entity || !token.generation || TrackedEntities().IsDisposed(token) ||
            TrackedEntities().IsRetiring(token) || owner.OwnedToken(token.entity).generation != token.generation)
            return false;
        const auto actual = entities.Read(token.entity);
        return actual.owner_world.read.returned && actual.owner_world.value == world &&
               !TrackedEntities().IsDisposed(token) && !TrackedEntities().IsRetiring(token);
    }
    bool Release() {
        for (auto it = held.begin(); it != held.end();) {
            if (roots.Release(*it).complete) {
                roots.Forget(*it);
                it = held.erase(it);
            } else
                ++it;
        }
        return held.empty();
    }
    struct Bound {
        std::shared_ptr<State> factory;
        EntityToken token;
        GameWorld* world;
        void* model;
        void* art;
        ManagedRoots roots;
        std::map<void*, ManagedRootId> kept;
        std::map<std::string, void*> keys;
        std::map<GameEntity*, EntityOrigin> known_targets;
        void* face = nullptr;
        void* cache = nullptr;
        bool closed = false;
        Bound(std::shared_ptr<State> f, EntityToken t, GameWorld* w, void* m, void* a)
            : factory(std::move(f)), token(t), world(w), model(m), art(a), roots(factory->references) {}
        void Keep(void* object) {
            if (!object || kept.count(object))
                return;
            const auto id = roots.Keep(object);
            kept.emplace(object, id);
            if (!roots.Status(id).ready)
                throw std::runtime_error("model presentation object retention failed");
        }
        bool Current() const {
            auto& f = *factory;
            if (closed || !f.Owned(token, world))
                return false;
            const auto actual = f.entities.Component(token.entity, f.model_type);
            if (!actual.call.returned || actual.object != model)
                return false;
            const auto view = f.models.Read(static_cast<CharacterModelComponent*>(model));
            return view.changing.read.returned && !view.changing.value && view.art.read.returned &&
                   view.art.value == art && f.Owned(token, world);
        }
        void Check() const {
            if (!Current())
                throw std::runtime_error("model presentation actor/model changed");
        }
        void* Component(void* type) {
            Check();
            const auto value = factory->entities.Component(token.entity, type);
            if (!value.call.returned)
                throw std::runtime_error("model presentation component unreadable");
            Keep(value.object);
            return value.object;
        }
        void FaceReady() {
            auto& f = *factory;
            Check();
            if (!f.face_error.empty())
                throw ModelPresentationUnsupported(f.face_error);
            if (f.ensure_face)
                f.ensure_face(token, world);
            auto* actual = Component(f.face_type);
            if (!actual)
                throw ModelPresentationUnsupported("face material component absent");
            auto* material = Read<void*>(actual, f.face_cache);
            if (!material)
                throw ModelPresentationUnsupported("face material cache not initialized");
            Keep(material);
            if (face && (face != actual || cache != material))
                throw std::runtime_error("face material component/cache replaced");
            face = actual;
            cache = material;
            Check();
        }
        FaceMaterialValue Face() {
            FaceReady();
            auto& f = *factory;
            return {Read<float>(face, f.cheek), Read<float>(face, f.shy), Read<float>(face, f.shadow)};
        }
        void SetFace(FaceMaterialTransition input) {
            FaceReady();
            if (!WriteFaceMaterial(factory->face_set, face, input).returned)
                throw std::runtime_error("face material transition failed; application may be partial");
        }
        void Factor(const std::string& key, float value) {
            auto& f = *factory;
            Check();
            if (!f.scale_error.empty())
                throw ModelPresentationUnsupported(f.scale_error);
            auto name = keys.find(key);
            if (name == keys.end()) {
                const auto text = f.metadata->NewStringUtf8(key.c_str());
                if (!text.call.returned || !text.object)
                    throw std::runtime_error("model scale key allocation failed");
                Keep(text.object);
                name = keys.emplace(key, text.object).first;
            }
            Check();
            if (!WriteModelScaleFactor(f.scale_set, model, name->second, value).returned)
                throw std::runtime_error("model scale factor failed; application may be partial");
        }
        float Scale() {
            Check();
            auto& f = *factory;
            if (!f.scale_error.empty())
                throw ModelPresentationUnsupported(f.scale_error);
            float value = 0;
            if (!native::Call(f.scale_read, &value, model).returned)
                throw std::runtime_error("model scale unreadable");
            return value;
        }
        void Group() {
            auto& f = *factory;
            if (!f.group_type || !f.members)
                throw ModelPresentationUnsupported("model scale Group declaration unavailable");
            auto* group = Component(f.group_type);
            if (!group)
                return;
            void* list = nullptr;
            if (!native::Call(f.group_members, &list, group).returned)
                throw std::runtime_error("scale follow group unreadable");
            if (!list)
                return;
            Keep(list);
            const auto count = f.members->Count(list);
            if (!count.read.returned || count.value < 0)
                throw std::runtime_error("scale follow group count unreadable");
            if (count.value)
                throw ModelPresentationUnsupported("scale follow Group propagation is outside this unit");
        }
        void* FollowComponent(bool create) {
            auto& f = *factory;
            Check();
            if (!f.follow_error.empty())
                throw ModelPresentationUnsupported(f.follow_error);
            Group();
            auto* component = Component(f.follow_type);
            if (!component && create) {
                const auto made = f.follow_factory->Invoke(token.entity);
                if (!made.call.returned || !made.object)
                    throw std::runtime_error("scale follow component creation failed");
                component = made.object;
                Keep(component);
                GameEntity* actual = nullptr;
                if (!native::Call(f.component_owner, &actual, component).returned || actual != token.entity)
                    throw std::runtime_error("scale follow component owner differs");
                Check();
                if (!native::CallVoid(f.follow_initialize, component).returned)
                    throw std::runtime_error("scale follow initialization failed");
            }
            if (!component)
                return nullptr;
            if (Read<void*>(component, f.follow_point) || Read<void*>(component, f.follow_multiplier) ||
                Read<bool>(component, f.follow_late) || Read<bool>(component, f.follow_outline))
                throw ModelPresentationUnsupported(
                    "scale follow attachment/Multiplier/outline/LateUpdate branch is outside this unit");
            if (Read<void*>(component, f.follow_model) != model)
                throw std::runtime_error("scale follow is bound to a different model");
            return component;
        }
        EntityOrigin Target() {
            auto* component = FollowComponent(false);
            if (!component)
                return {};
            auto* entity = Read<GameEntity*>(component, factory->follow_target);
            if (!entity)
                return {};
            Keep(entity);
            const auto own = factory->owner.OwnedToken(entity);
            if (own.generation && factory->Owned(own, world)) {
                const EntityOrigin origin{own, world};
                known_targets[entity] = origin;
                return origin;
            }
            // A target we previously owned may retire before its follower.
            // Keep its historical identity so withdrawal can stop the driver;
            // this is not permission to invoke the retired target.
            if (const auto prior = known_targets.find(entity); prior != known_targets.end())
                return prior->second;
            throw ModelPresentationUnsupported("existing scale follow target is not a current owned actor");
        }
        void Follow(EntityOrigin target) {
            auto& f = *factory;
            if (target.token.entity == token.entity && target.token.entity)
                throw ModelPresentationUnsupported("scale follow requires a distinct model target");
            // Read before replacing: an externally owned active slot is not
            // silently captured as this consumer's baseline.
            Target();
            auto* component = FollowComponent(bool(target.token.entity));
            if (!component)
                return;
            if (target.token.entity && !f.Owned(target.token, target.world))
                throw std::runtime_error("scale follow target ended");
            if (target.token.entity && target.world != world)
                throw std::runtime_error("scale follow target belongs to another world");
            Keep(target.token.entity);
            if (target.token.entity)
                known_targets[target.token.entity] = target;
            Check();
            if (!WriteModelScaleFollow(f.follow_set, component, target.token.entity).returned)
                throw std::runtime_error("model scale follow failed; application may be partial");
        }
        bool Close() {
            closed = true;
            for (auto it = kept.begin(); it != kept.end();) {
                if (roots.Release(it->second).complete) {
                    roots.Forget(it->second);
                    it = kept.erase(it);
                } else
                    ++it;
            }
            return kept.empty();
        }
    };
    static std::shared_ptr<Bound> Capture(std::shared_ptr<State> s, EntityOrigin origin) {
        const auto token = origin.token;
        auto* world = origin.world;
        if (!s->Owned(token, world))
            throw std::runtime_error("model presentation requires a current owned actor");
        const auto model = s->entities.Component(token.entity, s->model_type);
        if (!model.call.returned)
            throw std::runtime_error("model presentation model unreadable");
        if (!model.object)
            return {};
        const auto snapshot = s->models.Read(static_cast<CharacterModelComponent*>(model.object));
        if (!snapshot.art.read.returned || !snapshot.changing.read.returned)
            throw std::runtime_error("model presentation Art state unreadable");
        if (!snapshot.art.value || snapshot.changing.value)
            return {};
        auto bound = std::make_shared<Bound>(s, token, world, model.object, snapshot.art.value);
        try {
            bound->Keep(token.entity);
            bound->Keep(world);
            bound->Keep(model.object);
            bound->Keep(snapshot.art.value);
            if (!bound->Current()) {
                if (!bound->Close())
                    s->pending.push_back(bound);
                return {};
            }
            return bound;
        } catch (...) {
            if (!bound->Close())
                s->pending.push_back(bound);
            throw;
        }
    }
};
ActorModelPresentationFactory::ActorModelPresentationFactory(RuntimeMetadata& m, ReferenceMethods refs, EntityAccess e,
                                                             ModelAccess models, ManagedArrays arrays,
                                                             EntityOwner owner,
                                                             std::function<void(EntityToken, GameWorld*)> face)
    : state_(std::make_shared<State>(m, refs, std::move(e), std::move(models), std::move(arrays), std::move(owner),
                                     std::move(face))) {
    auto& s = *state_;
    try {

        static_cast<ModelPresentationBindings&>(s) = ResolveModelPresentationBindings(
            m, [&](const char* ns, const char* name) { return s.Type(ns, name); },
            [&](void* owner_type, void* follow_type) {
                s.follow_factory = std::make_unique<GenericFactory>(ResolveGenericFactoryMethods(m), refs, m, s.arrays);
                if (!PrepareModelFollowFactory(*s.follow_factory, owner_type, follow_type))
                    throw std::runtime_error(s.follow_factory->Error());
            });
        try {
            s.scale_program = std::make_shared<ModelScaleNative>(m, refs);
        } catch (const std::exception& error) {
            s.scale_program_error = error.what();
        }
    } catch (...) {
        Close();
        throw;
    }
}
ActorModelPresentationFactory::~ActorModelPresentationFactory() = default;
std::unique_ptr<ActorModelPresentation> ActorModelPresentationFactory::Create(EntityToken token, GameWorld* world) {
    auto s = state_;
    auto bound = State::Capture(s, {token, world});
    if (!bound)
        return {};
    try {
        return std::make_unique<ActorModelPresentation>(ActorModelPresentationServices{
            [bound] { return bound->Current(); }, [bound] { return bound->Face(); },
            [bound](auto value) { bound->SetFace(value); },
            [bound](const auto& key, float value) { bound->Factor(key, value); }, [bound] { return bound->Scale(); },
            [bound] { return bound->Target(); },
            [s, world](EntityOrigin target) { return target.world == world && s->Owned(target.token, world); },
            [bound](EntityOrigin target) { bound->Follow(target); }, [bound] { return bound->Close(); },
            reinterpret_cast<std::uintptr_t>(bound->model)});
    } catch (...) {
        if (!bound->Close())
            s->pending.push_back(bound);
        throw;
    }
}
std::unique_ptr<ModelScaleProgram> ActorModelPresentationFactory::CreateScaleProgram() {
    auto s = state_;
    if (!s->scale_program)
        throw ModelPresentationUnsupported("character scale binding unavailable: " + s->scale_program_error);
    return std::make_unique<ModelScaleProgram>(
        ModelScaleProgramServices{[s](EntityOrigin origin) -> std::optional<ModelScaleTarget> {
            if (!s->Owned(origin.token, origin.world))
                throw ModelPresentationUnsupported("character scale requires a current owned actor target");
            auto bound = State::Capture(s, origin);
            if (!bound)
                return {};
            try {
                return s->scale_program->Bind(
                    origin, bound->model, bound->art, [bound] { return bound->Current(); },
                    [bound] {
                        bound->Check();
                        bound->Group();
                    },
                    [bound] { return bound->Close(); });
            } catch (...) {
                if (!bound->Close())
                    s->pending.push_back(bound);
                throw;
            }
        }});
}
bool ActorModelPresentationFactory::Close() {
    auto& s = *state_;
    s.closed = true;
    for (auto it = s.pending.begin(); it != s.pending.end();)
        if ((*it)->Close())
            it = s.pending.erase(it);
        else
            ++it;
    const bool scale = !s.scale_program || s.scale_program->Close();
    const bool factory = !s.follow_factory || s.follow_factory->Close(), refs = s.Release();
    return scale && factory && refs && s.pending.empty();
}
}
