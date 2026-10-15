#include <hksr/entity/NpcCreationSession.hpp>
#include <algorithm>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct NpcCreationSession::State {
    EntityOwner owner;
    EntityFactory factory;
    EntityAccess access;
    ManagedRoots roots;
    NpcCreationStatus status;
    std::vector<ManagedRootId> handles;
    GameWorld* world = nullptr;
    EntityCreationScope::BirthObserver observe_birth;
    State(const EntityOwner& client, Method create, EntityAccessMethods methods, ReferenceMethods references,EntityCreationScope::BirthObserver observer)
        : owner(client), factory(create), access(std::move(methods)), roots(references),observe_birth(std::move(observer)) {}
    bool Keep(void* object) {
        // Allocate bookkeeping before any managed side effect.
        handles.emplace_back();
        auto& id = handles.back();
        id = roots.Keep(object);
        return roots.Status(id).ready;
    }
    void ReleaseRoots() {
        bool complete = true;
        for (const auto id : handles) if (id && !roots.Release(id).complete) complete = false;
        status.closed = complete;
    }
    void FinishDisposed() {
        bool complete = true;
        for (auto& entity : status.entities) {
            if (!entity.disposed) entity.disposed = TrackedEntities().IsDisposed(entity.token);
            if (!entity.disposed) complete = false;
        }
        // Incomplete receipt means unknown created objects may still depend
        // on the inputs. Record that fact, do not claim all ownership closed.
        if (complete && !status.creation.incomplete) {
            ReleaseRoots();
            if (status.closed)
                for (const auto& entity : status.entities) TrackedEntities().Forget(entity.token);
        }
    }
};
NpcCreationSession::NpcCreationSession(const EntityOwner& owner, Method factory, EntityAccessMethods access, ReferenceMethods references,EntityCreationScope::BirthObserver observer)
    : state_(new State(owner, factory, std::move(access), references,std::move(observer))) {}
NpcCreationSession::~NpcCreationSession() { delete state_; }
const NpcCreationStatus& NpcCreationSession::Status() const { return state_->status; }
void NpcCreationSession::Create(const NpcCreateInput& input) {
    auto& s = *state_;
    if (s.status.create_started) throw std::logic_error("NPC creation epoch is single-use");
    if (!input.world || !input.manager || !input.definition || !input.row)
        throw std::invalid_argument("NPC creation needs world, manager, private definition and row");
    s.status.create_started = true;
    s.world = input.world;
    bool kept = true;
    for (void* value : {static_cast<void*>(input.world), static_cast<void*>(input.manager),
        static_cast<void*>(input.definition), static_cast<void*>(input.row)}) {
        if (!s.Keep(value)) kept = false;
    }
    if (!kept) {
        s.status.errors.push_back("creation inputs could not all be retained");
        s.ReleaseRoots();
        return;
    }
    EntityCreationScope scope(input.world,s.owner.Identity(),s.observe_birth);
    s.status.factory = s.factory.CreateMapNpc(input.manager, input.definition, input.row, nullptr, input.factory_option);
    s.status.creation = scope.Receipt();
    s.status.create_returned = s.status.factory.call.returned && s.status.factory.entity;
    auto candidates = s.status.creation.entities;
    // A normal official factory return is a product result, not a diagnostic
    // guess. Synchronous inner creations additionally preserve failure cleanup.
    if (s.status.create_returned && std::find(candidates.begin(), candidates.end(), s.status.factory.entity) == candidates.end())
        candidates.push_back(s.status.factory.entity);
    s.status.entities.reserve(candidates.size());
    for (auto* entity : candidates) {
        const auto& disposed = s.status.creation.disposed_entities;
        if (std::find(disposed.begin(), disposed.end(), entity) != disposed.end()) {
            if (entity == s.status.factory.entity) s.status.create_returned = false;
            continue;
        }
        if (TrackedEntities().Current(entity).entity) {
            s.status.errors.push_back("factory exposed an entity already owned by another epoch; not adopted");
            if (entity == s.status.factory.entity) s.status.create_returned = false;
            continue;
        }
        const auto token = TrackedEntities().Track(entity,s.owner.Identity());
        s.status.entities.push_back({token, {}, {}, false, false});
        if (!s.Keep(entity)) {
            s.status.errors.push_back("new entity GC retention failed; disposal responsibility retained");
            s.status.removal_requested = true;
        }
        // Also cover a normally returned outer object when no inner creation
        // hook was observed. During inner creation the same token was already
        // published; provenance storage treats that as an idempotent receipt.
        if(s.observe_birth)try{s.observe_birth({token,s.world});}
        catch(const std::exception& error){s.status.errors.push_back(error.what());s.status.removal_requested=true;}
    }
    if (!s.status.create_returned || s.status.creation.incomplete) s.status.removal_requested = true;
    if (s.status.creation.incomplete) s.status.errors.push_back("creation footprint incomplete; known entities retained separately");
    if (s.status.entities.empty() && !s.status.creation.incomplete) s.ReleaseRoots();
}
void NpcCreationSession::Remove() { state_->status.removal_requested = true; }
void NpcCreationSession::Service(const WorldFrame& frame) {
    auto& s = *state_;
    if (!s.status.create_started || s.status.closed) return;
    // Observe natural disposal even when the old world is no longer serviced.
    // This performs no method call on the old entity/world/model.
    s.FinishDisposed();
    if (s.status.closed || frame.world != s.world) return;
    if (!frame.disposing.read.returned || frame.disposing.value) return;
    for (auto& entity : s.status.entities) {
        // An earlier deletion can synchronously dispose a dependent entity
        // from the same creation footprint. Do not call it a second time.
        if (!entity.disposed) entity.disposed = TrackedEntities().IsDisposed(entity.token);
        if (entity.disposed) continue;
        if (s.status.removal_requested) {
            if (!entity.retirement_started) {
                // Set before the native call: an exception may occur after
                // side effects. A later frame must not resubmit it silently.
                entity.retirement_started = true;
                entity.retirement = s.access.RequestRetirement(s.world, entity.token.entity);
            }
        } else {
            entity.observation = s.access.Read(entity.token.entity);
        }
    }
    s.FinishDisposed();
}
}
