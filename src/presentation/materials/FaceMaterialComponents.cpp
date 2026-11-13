#include "FaceMaterialComponents.hpp"
#include "../../../adapters/presentation/FaceMaterialBindings.hpp"
#include <hksr/managed/GenericFactory.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include "../../runtime/RuntimeResident.hpp"

namespace hksr::api::detail {
struct FaceMaterialComponents::State : FaceMaterialBindings {
    std::shared_ptr<RuntimeMetadata> metadata;
    EntityAccess entities;
    EntityOwner owner;
    ManagedRoots roots;
    GenericFactory factory;
    std::vector<ManagedRootId> handles;
    bool closed = false;
    State(RuntimeMetadata& m, ReferenceMethods refs, EntityAccessMethods access, ManagedArrays arrays, EntityOwner own)
        : metadata(RetainRuntimeMetadata(m)), entities(std::move(access)), owner(std::move(own)), roots(refs),
          factory(ResolveGenericFactoryMethods(m), refs, m, std::move(arrays)) {}
    bool Current(EntityToken token, GameWorld* world) const {
        if (closed || !token.entity || !token.generation ||
            owner.OwnedToken(token.entity).generation != token.generation || TrackedEntities().IsDisposed(token) ||
            TrackedEntities().IsRetiring(token))
            return false;
        const auto value = entities.Read(token.entity);
        return value.owner_world.read.returned && value.owner_world.value == world &&
               !TrackedEntities().IsDisposed(token);
    }
};
FaceMaterialComponents::FaceMaterialComponents(RuntimeMetadata& metadata, ReferenceMethods refs,
                                               EntityAccessMethods access, ManagedArrays arrays, EntityOwner owner)
    : state_(std::make_unique<State>(metadata, refs, std::move(access), std::move(arrays), std::move(owner))) {
    auto& s = *state_;
    try {

        static_cast<FaceMaterialBindings&>(s) = ResolveFaceMaterialBindings(metadata, [&s](ObjectResult value) {
            if (!value.call.returned || !value.object)
                throw std::runtime_error("face material Type unavailable");
            s.handles.push_back(s.roots.Keep(value.object));
            if (!s.roots.Status(s.handles.back()).ready)
                throw std::runtime_error("face material Type retention failed");
            return value.object;
        });
        if (!s.factory.Prepare(s.entity_type, "CreateComponent", s.face_type))
            throw std::runtime_error(s.factory.Error());
    } catch (...) {
        Close();
        throw;
    }
}
FaceMaterialComponents::~FaceMaterialComponents() = default;
void FaceMaterialComponents::Ensure(EntityToken token, GameWorld* world) {
    auto& s = *state_;
    auto current = [&] { return s.Current(token, world); };
    EnsureFaceMaterialComponent(
        {current, [&] { return s.entities.Component(token.entity, s.face_type); },
         [&] { return s.factory.Invoke(token.entity); },
         [&](void* component) {
             GameEntity* owner = nullptr;
             if (!native::Call(s.component_owner, &owner, component).returned || owner != token.entity)
                 throw std::runtime_error("created face material component belongs to a different entity");
             if (!current())
                 throw std::runtime_error("face material owner ended during initialization");
             const auto model = s.entities.Component(token.entity, s.model_type);
             if (!model.call.returned)
                 throw std::runtime_error("face material model lookup failed");
             if (!model.object)
                 return; // Normal model-loaded callback will initialize it.
             bool loaded = false;
             if (!native::Call(s.art_loaded, &loaded, model.object).returned)
                 throw std::runtime_error("face material model state unread");
             if (!loaded || !current())
                 return;
             void* cache = nullptr;
             if (!native::CopyMemoryChecked(&cache, static_cast<char*>(component) + s.cache_offset, sizeof(cache)))
                 throw std::runtime_error("created face material cache unread");
             // CreateComponent may have already delivered the model callback.
             // Late insertion into an already loaded actor needs that callback once.
             if (!cache && !native::CallVoid(s.on_art_loaded, component).returned)
                 throw std::runtime_error("face material model initialization failed");
         }});
}
bool FaceMaterialComponents::Close() {
    auto& s = *state_;
    s.closed = true;
    bool complete = s.factory.Close();
    for (auto it = s.handles.begin(); it != s.handles.end();) {
        if (s.roots.Release(*it).complete) {
            s.roots.Forget(*it);
            it = s.handles.erase(it);
        } else {
            complete = false;
            ++it;
        }
    }
    return complete;
}
}
