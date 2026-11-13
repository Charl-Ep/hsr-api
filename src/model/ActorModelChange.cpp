#include "../../adapters/model/ModelBindings.hpp"
#include <hksr/model/ModelAccess.hpp>
#include <hksr/model/ActorModelChange.hpp>
#include "ActorModelChangeNative.hpp"
#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/runtime/OperationTrace.hpp>
#include "../runtime/RuntimeResident.hpp"
#include <map>
#include <stdexcept>

namespace hksr::api {
struct ActorModelChange::State {
    ActorModelChangeServices services;
    ActorModelChangeStatus status;
    bool busy = false, closing = false, cancelled = false;
    explicit State(ActorModelChangeServices value) : services(std::move(value)) {
        if (!services.current || !services.resolve || !services.change || !services.close)
            throw std::invalid_argument("model change services incomplete");
    }
};
ActorModelChange::ActorModelChange(ActorModelChangeServices services)
    : state_(std::make_unique<State>(std::move(services))) {}
ActorModelChange::~ActorModelChange() = default;
const ActorModelChangeStatus& ActorModelChange::Status() const {
    return state_->status;
}
void ActorModelChange::Service() {
    auto& s = *state_;
    auto& status = s.status;
    if (s.busy || s.closing || status.closed || status.phase != ActorModelChangePhase::Pending)
        return;
    OperationSpan timing("sdk.model.change");
    s.busy = true;
    struct Exit {
        bool& value;
        ~Exit() {
            value = false;
        }
    } exit{s.busy};
    try {
        if (!s.services.current())
            throw std::runtime_error("model change owned actor/world/model component ended");
        status.resolution = s.services.resolve();
        if (!status.resolution.call.returned)
            throw std::runtime_error("official model target resolution failed");
        if (s.cancelled || s.closing) {
            status.phase = ActorModelChangePhase::Cancelled;
            return;
        }
        if (!s.services.current())
            throw std::runtime_error("model change owner changed during resolution");
        if (s.cancelled || s.closing) {
            status.phase = ActorModelChangePhase::Cancelled;
            return;
        }
        if (!status.resolution.changed) {
            status.phase = ActorModelChangePhase::Unchanged;
            return;
        }
        status.submitted = true;
        status.may_have_changed = true; // record before the native side effect
        status.change = s.services.change();
        if (!status.change.call.returned)
            throw std::runtime_error("official model change failed; model may already have changed");
        if (!s.services.current())
            throw std::runtime_error("model change owner ended during submission");
        status.may_have_changed = status.change.changed;
        status.phase = status.change.changed ? ActorModelChangePhase::Ready : ActorModelChangePhase::Unchanged;
    } catch (const std::exception& e) {
        status.error = e.what();
        status.phase = ActorModelChangePhase::Failed;
    }
}
bool ActorModelChange::Cancel() {
    auto& s = *state_;
    if (s.status.submitted || s.status.phase != ActorModelChangePhase::Pending)
        return false;
    s.cancelled = true;
    if (!s.busy)
        s.status.phase = ActorModelChangePhase::Cancelled;
    return true;
}
bool ActorModelChange::Close() {
    auto& s = *state_;
    if (s.status.closed)
        return true;
    s.closing = true;
    Cancel();
    if (s.busy)
        return false;
    try {
        s.status.closed = s.services.close();
        s.status.cleanup_error = s.status.closed ? "" : "model change input references pending";
        return s.status.closed;
    } catch (const std::exception& e) {
        s.status.cleanup_error = e.what();
        return false;
    }
}
}

namespace hksr::api::detail {
struct ActorModelChangeFactory::State {
    std::shared_ptr<RuntimeMetadata> metadata;
    ManagedObjects* objects;
    ReferenceMethods references;
    EntityAccess entities;
    ModelAccess models;
    EntityOwner owner;
    std::function<bool(EntityToken, GameWorld*)> owns;
    ManagedRoots roots;
    void* model_type = nullptr;
    ManagedRootId type_root = 0;
    ModelProfile profile{ModelMethods{}};
    bool closed = false;
    State(ManagedObjects& o, ReferenceMethods refs, EntityAccess e, ModelAccess models_, EntityOwner ownership)
        : objects(&o), references(refs), entities(std::move(e)), models(std::move(models_)),
          owner(std::move(ownership)), roots(refs) {}
    bool Owned(EntityToken token, GameWorld* world) {
        return !closed && token.entity && token.generation && world && !TrackedEntities().IsDisposed(token) &&
               !TrackedEntities().IsRetiring(token) &&
               TrackedEntities().Current(token.entity).generation == token.generation &&
               (owns ? owns(token, world) : owner.OwnedToken(token.entity).generation == token.generation);
    }
    struct Bound {
        std::shared_ptr<State> factory;
        EntityToken token;
        GameWorld* world;
        void* model;
        void* path = nullptr;
        ManagedRoots roots;
        std::map<void*, ManagedRootId> held;
        bool closed = false;
        Bound(std::shared_ptr<State> f, EntityToken t, GameWorld* w, void* m)
            : factory(std::move(f)), token(t), world(w), model(m), roots(factory->references) {}
        void Keep(void* object) {
            if (!object || held.count(object))
                return;
            auto row = held.emplace(object, 0).first;
            const auto id = roots.Keep(object);
            row->second = id;
            if (!roots.Status(id).ready)
                throw std::runtime_error("model change input retention failed");
        }
        bool Current() {
            auto& f = *factory;
            if (closed || !f.Owned(token, world))
                return false;
            const auto entity = f.entities.Read(token.entity);
            if (!entity.owner_world.read.returned)
                throw std::runtime_error("model change actor world unreadable");
            if (entity.owner_world.value != world)
                return false;
            const auto current = f.entities.Component(token.entity, f.model_type);
            if (!current.call.returned)
                throw std::runtime_error("model change component unreadable");
            return current.object == model;
        }
        ChangeResult Resolve() {
            if (!Current())
                throw std::runtime_error("model change actor/model ended before resolution");
            void* resolved = nullptr;
            // The synchronous official task passes its original nullable input
            // to ChangeModel. The resolved by-ref path is not a second writer.
            return factory->profile.CanChange(model, path, &resolved);
        }
        ChangeResult Change() {
            if (!Current())
                throw std::runtime_error("model change actor/model ended before submission");
            return factory->profile.Change(model, path, nullptr);
        }
        bool Close() {
            closed = true;
            for (auto it = held.begin(); it != held.end();)
                if (!it->second)
                    it = held.erase(it);
                else if (roots.Release(it->second).complete) {
                    roots.Forget(it->second);
                    it = held.erase(it);
                } else
                    ++it;
            return held.empty();
        }
    };
    std::vector<std::shared_ptr<Bound>> pending;
};
ActorModelChangeFactory::ActorModelChangeFactory(RuntimeMetadata& m, ReferenceMethods refs, EntityAccess entities,
                                                 ModelAccess models, EntityOwner owner)
    : ActorModelChangeFactory(m, m, refs, std::move(entities), std::move(models), std::move(owner), {}) {
    state_->metadata = RetainRuntimeMetadata(m);
}
ActorModelChangeFactory::ActorModelChangeFactory(MethodResolver& methods, ManagedObjects& objects,
                                                 ReferenceMethods refs, EntityAccess entities, ModelAccess models,
                                                 EntityOwner owner, std::function<bool(EntityToken, GameWorld*)> owns)
    : state_(std::make_shared<State>(objects, refs, std::move(entities), std::move(models), std::move(owner))) {
    auto& s = *state_;
    s.owns = std::move(owns);
    try {
        const auto type = detail::ResolveCharacterModelType(objects);
        if (!type.call.returned || !type.object)
            throw std::runtime_error("model change model Type unavailable");
        s.type_root = s.roots.Keep(type.object);
        if (!s.roots.Status(s.type_root).ready)
            throw std::runtime_error("model change model Type retention failed");
        s.model_type = type.object;
        s.profile = ModelProfile(ResolveModelChangeBindings(methods));
    } catch (...) {
        Close();
        throw;
    }
}
ActorModelChangeFactory::~ActorModelChangeFactory() = default;
std::unique_ptr<ActorModelChange> ActorModelChangeFactory::Create(EntityToken token, GameWorld* world,
                                                                  std::optional<std::string> path) {
    auto f = state_;
    if (path && path->find('\0') != std::string::npos)
        throw std::invalid_argument("model change path contains NUL");
    if (!f->Owned(token, world))
        throw std::invalid_argument("model change requires a current owned actor");
    const auto entity = f->entities.Read(token.entity);
    if (!entity.owner_world.read.returned || entity.owner_world.value != world)
        throw std::runtime_error("model change actor world differs");
    const auto model = f->entities.Component(token.entity, f->model_type);
    if (!model.call.returned)
        throw std::runtime_error("model change model lookup failed");
    if (!model.object)
        return {};
    auto bound = std::make_shared<State::Bound>(f, token, world, model.object);
    try {
        bound->Keep(token.entity);
        bound->Keep(world);
        bound->Keep(model.object);
        bound->Keep(f->model_type);
        if (path) {
            const auto text = f->objects->NewStringUtf8(path->c_str());
            if (!text.call.returned || !text.object)
                throw std::runtime_error("model change path allocation failed");
            bound->path = text.object;
            bound->Keep(text.object);
        }
        return std::make_unique<ActorModelChange>(
            ActorModelChangeServices{[bound] { return bound->Current(); }, [bound] { return bound->Resolve(); },
                                     [bound] { return bound->Change(); }, [bound] { return bound->Close(); }});
    } catch (...) {
        if (!bound->Close())
            f->pending.push_back(bound);
        throw;
    }
}
bool ActorModelChangeFactory::Close() {
    auto& s = *state_;
    s.closed = true;
    for (auto it = s.pending.begin(); it != s.pending.end();)
        if ((*it)->Close())
            it = s.pending.erase(it);
        else
            ++it;
    if (s.type_root && s.roots.Release(s.type_root).complete) {
        s.roots.Forget(s.type_root);
        s.type_root = 0;
    }
    return !s.type_root && s.pending.empty();
}
}
