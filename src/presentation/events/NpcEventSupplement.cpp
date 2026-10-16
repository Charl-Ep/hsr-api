#include <hksr/presentation/events/NpcEventSupplement.hpp>
#include <hksr/runtime/OperationTrace.hpp>
#include <hksr/presentation/events/NpcEventBindings.hpp>
#include <hksr/entity/EntityCreationSession.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct NpcEventSupplement::State {
    State(std::function<const NpcCreationStatus&()>,GameWorld*,ReferenceMethods,NpcEventBindings,std::size_t=0);
    void Prepare(AnimationInputTypes,ObjectResult);
    void Service(const WorldFrame&);
    bool InputsDetached() const;
    bool Quiescent() const {return !active_ && retired_.empty();}
    struct Identity {void* component=nullptr;void* consumer=nullptr;void* context=nullptr;void* baseline=nullptr;};
    void* Keep(ObjectResult,bool nullable=false);
    void Drain(bool owner_ended,bool own_frame);
    void RetireActive(bool superseded);
    std::function<const NpcCreationStatus&()> creation_;
    std::size_t story_baseline_offset_=0;
    EntityCreationSession* entity_=nullptr;
    GameWorld* world_;
    ReferenceMethods refs_;
    NpcEventBindings services_;
    ManagedRoots roots_;
    std::vector<ManagedRootId> handles_;
    void* data_type_=nullptr;
    void* event_type_=nullptr;
    ObjectResult desired_;
    Identity identity_;
    bool preparation_attempted_=false;
    std::unique_ptr<EventRegistrationLease> active_;
    std::vector<std::unique_ptr<EventRegistrationLease>> retired_;
    NpcEventSupplementStatus status_;
};
NpcEventSupplement::NpcEventSupplement(NpcCreationSession& npc,GameWorld* world,ReferenceMethods refs,NpcEventBindings services)
    :state_(new State([&npc]()->const auto&{return npc.Status();},world,refs,std::move(services))){}
NpcEventSupplement::NpcEventSupplement(EntityCreationSession& entity,GameWorld* world,ReferenceMethods refs,NpcEventBindings services,std::size_t story_offset)
    :state_(new State([&entity]()->const auto&{return entity.Status();},world,refs,std::move(services),story_offset)){state_->entity_=&entity;}
NpcEventSupplement::~NpcEventSupplement(){delete state_;}
void NpcEventSupplement::Prepare(AnimationInputTypes types,ObjectResult paths){state_->Prepare(types,paths);}
void NpcEventSupplement::SetEnabled(bool value){state_->status_.enabled=value;}
void NpcEventSupplement::RequestClose(){state_->status_.closing=true;}
void NpcEventSupplement::Service(const WorldFrame& frame){state_->Service(frame);}
bool NpcEventSupplement::Quiescent() const{return state_->Quiescent();}
bool NpcEventSupplement::InputsDetached() const{return state_->InputsDetached();}
const NpcEventSupplementStatus& NpcEventSupplement::Status() const{return state_->status_;}
const EventRegistrationLease* NpcEventSupplement::Active() const{return state_->active_.get();}

namespace {
void Require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
}
NpcEventSupplement::State::State(std::function<const NpcCreationStatus&()> creation,api::GameWorld* world,api::ReferenceMethods refs,NpcEventBindings services,std::size_t story_offset)
    :creation_(std::move(creation)),story_baseline_offset_(story_offset),world_(world),refs_(refs),services_(std::move(services)),roots_(refs) {
    if(!world_) throw std::invalid_argument("NPC events require their owning world");
}
void* NpcEventSupplement::State::Keep(api::ObjectResult value,bool nullable) {
    Require(value.call.returned && (nullable || value.object),"NPC event source unavailable");
    if(!value.object) return nullptr;
    handles_.push_back(0); const auto id=roots_.Keep(value.object); handles_.back()=id;
    Require(roots_.Status(id).ready,"NPC event source root unavailable"); return value.object;
}
void NpcEventSupplement::State::Prepare(api::AnimationInputTypes types,api::ObjectResult desired) {
    OperationSpan timing("sdk.events.prepare");
    if(preparation_attempted_ || status_.closing) throw std::logic_error("NPC event sources are single-use");
    preparation_attempted_=true;
    try {
        data_type_=Keep(types.data); event_type_=Keep(types.events);
        Keep(desired,true); desired_=desired; status_.prepared=true;
    } catch(const std::exception& error) { status_.errors={error.what()}; }
}
void NpcEventSupplement::State::RetireActive(bool superseded) {
    if(!active_) return;
    retired_.reserve(retired_.size()+1);
    if(superseded) active_->Superseded();
    retired_.push_back(std::move(active_)); identity_={};
}
bool NpcEventSupplement::State::InputsDetached() const {
    if(active_) return false;
    for(const auto& input:retired_) {
        const auto& s=input->Status();
        if(s.attached && !s.retirement_complete && !s.owner_ended && !s.identity_changed) return false;
    }
    return true;
}
void NpcEventSupplement::State::Drain(bool owner_ended,bool own_frame) {
    OperationSpan timing("sdk.events.drain");
    for(auto it=retired_.begin();it!=retired_.end();) {
        auto& input=**it;
        if(owner_ended) input.OwnerEnded();
        const auto& state=input.Status();
        if(!state.owner_ended && !state.identity_changed && !state.retirement_complete) {
            if(!own_frame) { ++it; continue; }
            input.Retire();
        }
        if(input.Close().complete) {
            if(!state.retirement_complete) ++status_.incomplete_retirements;
            it=retired_.erase(it);
        } else ++it;
    }
    status_.retiring=retired_.size();
    if(status_.closing && InputsDetached()) {
        bool closed=true;
        for(auto id:handles_) if(id && !roots_.Release(id).complete) closed=false;
        status_.closed=closed && Quiescent();
    }
}
void NpcEventSupplement::State::Service(const api::WorldFrame& frame) {
    OperationSpan timing("sdk.events.service");
    if(status_.closed) return;
    const auto& npc=creation_();
    const api::NpcEntityReceipt* owner=nullptr;
    for(const auto& receipt:npc.entities) if(receipt.token.entity==npc.factory.entity) { owner=&receipt; break; }
    bool gone=npc.closed;
    if(owner) {
        const auto current=api::TrackedEntities().Current(owner->token.entity);
        gone=gone || owner->disposed || api::TrackedEntities().IsDisposed(owner->token) || current.generation!=owner->token.generation;
    }
    const bool own_frame=frame.world==world_ && frame.disposing.read.returned && !frame.disposing.value;
    if(gone || npc.removal_requested) status_.closing=true;
    if(gone || status_.closing || !status_.enabled) RetireActive(false);
    Drain(gone,own_frame);
    if(gone || status_.closing || !own_frame || !status_.enabled || !status_.prepared || !owner || !npc.create_returned) return;
    try {
        const auto entity=services_.entities.Read(owner->token.entity);
        if(!entity.owner_world.read.returned || entity.owner_world.value!=world_ || !entity.loaded.read.returned || !entity.loaded.value) return;
        status_.data_component=services_.entities.Component(owner->token.entity,data_type_);
        status_.event_component=services_.entities.Component(owner->token.entity,event_type_);
        Require(status_.data_component.call.returned && status_.data_component.object,"NPC baseline component unavailable");
        Require(status_.event_component.call.returned && status_.event_component.object,"NPC event component unavailable");
        status_.baseline_config=api::ReadAdventureDataConfig(status_.data_component.object,services_.data_field);
        Require(status_.baseline_config.call.returned && status_.baseline_config.object,"NPC baseline configuration unavailable");
        auto baseline=services_.config.Read(status_.baseline_config.object).event_paths;
        if(story_baseline_offset_ && entity_ && entity_->IsStory())baseline.call.returned=native::CopyMemoryChecked(&baseline.object,
            static_cast<unsigned char*>(status_.baseline_config.object)+story_baseline_offset_,sizeof(void*));
        Require(baseline.call.returned,"NPC baseline event paths unavailable");
        const auto consumer=services_.events.Consumer(status_.event_component.object);
        const auto context=services_.events.Context(status_.event_component.object);
        Require(consumer.call.returned && consumer.object && context.call.returned && context.object,"NPC event identity unavailable");
        Identity now{status_.event_component.object,consumer.object,context.object,baseline.object};
        if(active_) {
            const bool replaced=identity_.component!=now.component || identity_.consumer!=now.consumer || identity_.context!=now.context;
            if(replaced || identity_.baseline!=now.baseline) RetireActive(replaced);
        }
        Drain(false,true);
        // An unresolved retirement in this same consumer can overlap a newly
        // registered copy. It affects this event supplement, not other systems.
        for(const auto& old:retired_) {
            const auto& state=old->Status();
            if(state.attached && !state.owner_ended && !state.identity_changed && !state.retirement_complete) return;
        }
        if(!active_) {
            active_=std::make_unique<api::EventRegistrationLease>(refs_,services_.events,services_.lists);
            identity_=now; ++status_.incarnations;
            active_->Prepare(services_.arrays,services_.strings,baseline,desired_);
            if(active_->Status().prepared) active_->Attach(owner->token.entity,now.component,owner->token);
        }
        active_->Service();
        status_.errors=active_->Status().errors;
    } catch(const std::exception& error) { status_.errors={error.what()}; }
}
}
