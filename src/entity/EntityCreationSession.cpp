#include <hksr/entity/EntityCreationSession.hpp>
#include <hksr/runtime/OperationTrace.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <hksr/native/ActorEmotion.hpp>
#include <hksr/native/ActorInputPolicy.hpp>
#include <hksr/runtime/RuntimeHost.hpp>
#include <algorithm>
#include <stdexcept>
#include <sstream>

namespace hksr::sdk_v1 {
struct EntityCreationSession::State {
    EntityOwner owner;
    EntityFactory factory;
    Method story_factory;
    Method partner_factory;
    ManagedObjects* objects;
    bool story=false;
    bool constructing=false;
    std::unique_ptr<ActorEmotionBoundary> emotion_boundary;
    std::unique_ptr<ActorInputPolicy> outfit_boundary;
    void* story_identifier=nullptr;
    ActorEmotionMethods emotion_methods;
    ReferenceMethods references;
    EntityAccess access;
    ManagedRoots roots;
    NpcCreationStatus status;
    std::vector<ManagedRootId> handles;
    GameWorld* world = nullptr;
    EntityToken summoner;
    EntityCreationScope::BirthObserver observe_birth;
    State(const EntityOwner& client,Method create,Method story_method,ManagedObjects* managed,EntityAccessMethods methods,ReferenceMethods references,EntityCreationScope::BirthObserver observer,Method partner)
        :owner(client),factory(create),story_factory(story_method),partner_factory(partner),objects(managed),references(references),access(std::move(methods)),roots(references),observe_birth(std::move(observer)) {}
    bool SummonerLive(EntityToken token) const {
        const auto owned=owner.OwnedToken(token.entity);
        return token.entity && token.generation && owned.generation==token.generation &&
            !TrackedEntities().IsRetiring(token);
    }
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
EntityCreationSession::EntityCreationSession(const EntityOwner& owner,Method factory,Method story,ManagedObjects* objects,EntityAccessMethods access,ReferenceMethods references,EntityCreationScope::BirthObserver observer,Method partner)
    :state_(new State(owner,factory,story,objects,std::move(access),references,std::move(observer),partner)) {}
EntityCreationSession::~EntityCreationSession() {
    if(state_->emotion_boundary)state_->emotion_boundary->Stop();
    if(state_->outfit_boundary)state_->outfit_boundary->Stop();
    delete state_;
}
void EntityCreationSession::PrepareStoryRuntime(RuntimeMetadata& metadata) {
    OperationSpan timing("sdk.creation.bindings");
    auto& s=*state_;s.emotion_methods=ResolveActorEmotionMethods(metadata);
    std::size_t offset=0;
    for(const auto& field:metadata.ReadFields("RPG.GameCore","GameComponentBase",false))
        if(field.name=="_OwnerRef" && field.type=="RPG.GameCore.GameEntity")offset=field.offset;
    if(offset<16)throw std::runtime_error("entity component owner field unavailable");
    s.outfit_boundary=std::make_unique<ActorInputPolicy>(ActorInputKind::OutfitIdentity,
        metadata.Resolve("RPG.GameCore","CharacterOutfitComponent","InitComponentForStoryCharacter","System.Void",{"System.String"}),
        [this,offset](void* component,void* identifier)->std::optional<void*> {
            auto& state=*state_;
            if(identifier || !component || !state.story || !state.constructing || !state.story_identifier)return {};
            GameEntity* entity=nullptr;
            if(!native::CopyMemoryChecked(&entity,static_cast<unsigned char*>(component)+offset,sizeof(entity)) ||
                !EntityCreationScope::ContainsOwned(entity,state.owner.Identity()))return {};
            // The official initializer tests Boy/Girl substrings. For missing
            // FreeStyle IDs, use this actor's official prefab path as input;
            // do not fabricate an NPC ID or modify shared configuration.
            return state.story_identifier;
        });
    s.emotion_boundary=std::make_unique<ActorEmotionBoundary>(metadata.Resolve("","PEPNFLNCDHG","JHDMCDCFDDP","System.Void",{}),
        [this,offset](void* component) {
            auto& state=*state_;if(!state.story || !component)return false;
            GameEntity* entity=nullptr;
            if(!native::CopyMemoryChecked(&entity,static_cast<unsigned char*>(component)+offset,sizeof(entity)))return false;
            bool owned=state.constructing && EntityCreationScope::ContainsOwned(entity,state.owner.Identity());
            if(!owned)for(const auto& receipt:state.status.entities)
                if(receipt.token.entity==entity && !TrackedEntities().IsDisposed(receipt.token) &&
                    TrackedEntities().Current(entity).generation==receipt.token.generation){owned=true;break;}
            if(!owned)return false;
            // LocalPlayer/Avatar Animator remains the default expression driver.
            // This runs for each newly loaded model, not once per entity lifetime.
            ActorEmotionSetup setup(state.emotion_methods,state.references);
            setup.Apply(component,nullptr,nullptr,false);
            if(!setup.State().ready){state.status.errors.push_back(setup.State().error);state.status.removal_requested=true;}
            return true;
        });
}
const NpcCreationStatus& EntityCreationSession::Status() const { return state_->status; }
bool EntityCreationSession::IsStory() const {return state_->story;}
Method ResolveStoryActorFactory(MethodResolver& m) {
    return m.Resolve("RPG.GameCore","EntityManagerExtension","ClientCreateStoryNPCEntity","RPG.GameCore.GameEntity",
        {"RPG.GameCore.EntityManager","RPG.GameCore.NPCSubType","System.String","System.String","System.String","System.String",
         "RPG.GameCore.StoryCharacterRow","System.String","System.String","System.String[]","System.Boolean","System.Action<RPG.GameCore.GameEntity>"},MethodKind::Static);
}
Method ResolvePartnerFactory(MethodResolver& m) {
    return m.Resolve("RPG.GameCore","EntityManagerExtension","CreatePartner","RPG.GameCore.GameEntity",
        {"RPG.GameCore.EntityManager","RPG.GameCore.GameEntity","System.String","System.String"},MethodKind::Static);
}
void EntityCreationSession::Create(const NpcCreateInput& input) {
    if(!input.definition || !input.row)throw std::invalid_argument("map NPC requires its definition and row");
    CreateOwned(input.world,input.manager,false,[&] {
        if(!state_->Keep(input.definition) || !state_->Keep(input.row))throw std::runtime_error("NPC input retention failed");
        return state_->factory.CreateMapNpc(input.manager,input.definition,input.row,nullptr,input.factory_option);
    });
}
void EntityCreationSession::Create(const StoryCreateInput& input) {
    OperationSpan timing("sdk.creation.story");
    auto& s=*state_;
    if(s.status.create_started)throw std::logic_error("entity creation epoch is single-use");
    if(!s.objects || !s.story_factory.code || input.model_path.empty() || input.adventure_config.empty() || input.unique_name.empty())
        throw std::invalid_argument("story actor requires its factory, unique name and model/configuration paths");
    s.story=true;
    CreateOwned(input.world,input.manager,true,[&] {
        auto text=[&](const std::string& value) {
            auto result=s.objects->NewStringUtf8(value.c_str());
            if(!result.call.returned || !result.object || !s.Keep(result.object))throw std::runtime_error("story input string retention failed");
            return result.object;
        };
        auto* graph=text("");auto* model=text(input.model_path);auto* name=text(input.unique_name);auto* config=text(input.adventure_config);
        s.story_identifier=model;
        CreateEntityResult result;
        result.call=native::Call(s.story_factory,&result.entity,input.manager,4,graph,model,static_cast<void*>(nullptr),name,
            static_cast<void*>(nullptr),config,static_cast<void*>(nullptr),static_cast<void*>(nullptr),true,static_cast<void*>(nullptr));
        return result;
    });
}
void EntityCreationSession::Create(const PartnerCreateInput& input) {
    OperationSpan timing("sdk.creation.partner");
    auto& s=*state_;
    if(s.status.create_started)throw std::logic_error("entity creation epoch is single-use");
    if(!s.objects || !s.partner_factory.code || input.model_path.empty() || input.character_config.empty())
        throw std::invalid_argument("partner requires its factory and model/configuration paths");
    if(!s.SummonerLive(input.summoner))throw std::invalid_argument("partner summoner must be a live entity owned by this client");
    const auto parent=s.access.Read(input.summoner.entity);
    if(!parent.owner_world.read.returned || parent.owner_world.value!=input.world || !s.SummonerLive(input.summoner))
        throw std::invalid_argument("partner summoner is not in the creation world");
    s.summoner=input.summoner;
    CreateOwned(input.world,input.manager,true,[&] {
        if(!s.Keep(input.summoner.entity))throw std::runtime_error("partner summoner retention failed");
        auto text=[&](const std::string& value) {
            const auto result=s.objects->NewStringUtf8(value.c_str());
            if(!result.call.returned || !result.object || !s.Keep(result.object))throw std::runtime_error("partner input string retention failed");
            return result.object;
        };
        auto* model=text(input.model_path);auto* config=text(input.character_config);
        // Managed preparation can re-enter disposal. A retained reference
        // alone does not preserve the parent's engine lifetime.
        if(!s.SummonerLive(input.summoner))throw std::runtime_error("partner summoner ended during preparation");
        CreateEntityResult result;
        result.call=native::Call(s.partner_factory,&result.entity,input.manager,input.summoner.entity,model,config);
        return result;
    });
}
void EntityCreationSession::CreateOwned(GameWorld* world,EntityManager* manager,bool require_birth,const std::function<CreateEntityResult()>& create) {
    auto& s=*state_;
    if(s.status.create_started)throw std::logic_error("entity creation epoch is single-use");
    if(!world || !manager)throw std::invalid_argument("entity creation needs its world and manager");
    s.status.create_started=true;s.world=world;
    if(s.emotion_boundary)s.emotion_boundary->ActivateOnCurrentThread();
    if(s.outfit_boundary)s.outfit_boundary->ActivateOnCurrentThread();
    if(!s.Keep(world) || !s.Keep(manager)) {
        s.status.errors.push_back("creation inputs could not all be retained");s.ReleaseRoots();return;
    }
    EntityCreationScope scope(world,s.owner.Identity(),s.observe_birth);
    s.constructing=true;
    try {s.status.factory=create();}
    catch(const std::exception& e){s.status.errors.push_back(e.what());}
    if(s.status.factory.call.exception) {
        std::ostringstream message;message<<"official creation threw native exception 0x"<<std::hex<<s.status.factory.call.exception;
        s.status.errors.push_back(message.str());
    }
    s.constructing=false;
    s.status.creation = scope.Receipt();
    s.status.create_returned = s.status.factory.call.returned && s.status.factory.entity;
    auto candidates = s.status.creation.entities;
    // A normal official factory return is a product result, not a diagnostic
    // guess. Synchronous inner creations additionally preserve failure cleanup.
    if(s.status.create_returned && std::find(candidates.begin(),candidates.end(),s.status.factory.entity)==candidates.end()) {
        if(require_birth){s.status.create_returned=false;s.status.errors.push_back("independent factory returned an entity without a birth receipt");}
        else candidates.push_back(s.status.factory.entity);
    }
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
void EntityCreationSession::Remove() { state_->status.removal_requested = true; }
void EntityCreationSession::Service(const WorldFrame& frame) {
    auto& s = *state_;
    if(s.emotion_boundary)s.emotion_boundary->ActivateOnCurrentThread();
    if (!s.status.create_started || s.status.closed) return;
    if(s.summoner.entity && !s.SummonerLive(s.summoner))s.status.removal_requested=true;
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
