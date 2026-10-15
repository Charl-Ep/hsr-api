#include <hksr/model/ModelTransition.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct ModelTransition::State {
    EntityToken owner;
    GameWorld* world;
    std::unique_ptr<ModelProfileRequest> request;
    ModelTransitionServices services;
    ManagedRoots roots;
    std::vector<ManagedRootId> handles;
    void* model_type=nullptr;
    std::unique_ptr<ControllerBinding> controller;
    std::vector<std::unique_ptr<ControllerBinding>> retired;
    ModelTransitionPlan plan;
    ModelTransitionStatus status;
    bool busy=false,closing=false,initial_played=false,detach_before_adoption=false;
    std::uint64_t revision=0,initial_boundary=0;
    int initial_hash=0;
    State(EntityToken o,GameWorld* w,std::unique_ptr<ModelProfileRequest> r,ModelTransitionServices s)
        :owner(o),world(w),request(std::move(r)),services(std::move(s)),roots(services.references){}
    bool Ended() const {
        return TrackedEntities().IsDisposed(owner) || TrackedEntities().Current(owner.entity).generation!=owner.generation;
    }
    bool OwnFrame(const WorldFrame& frame) const {
        return frame.world==world && frame.disposing.read.returned && !frame.disposing.value;
    }
    bool Interrupted(std::uint64_t serial,const ModelTransitionConsumer& consumer) const {
        return closing || Ended() || revision!=serial || (consumer.interrupted && consumer.interrupted());
    }
    CharacterModelComponent* CurrentModel(ManagedObjects& objects,std::uint64_t serial,const ModelTransitionConsumer& consumer) {
        if(Interrupted(serial,consumer))return nullptr;
        if(!model_type) {
            const auto type=objects.ManagedType("RPG.GameCore","CharacterModelComponent");
            if(Interrupted(serial,consumer))return nullptr;
            if(!type.call.returned || !type.object)throw std::runtime_error("handoff model Type unavailable");
            const auto id=roots.Keep(type.object);handles.push_back(id);
            if(Interrupted(serial,consumer))return nullptr;
            if(!roots.Status(id).ready)throw std::runtime_error("handoff model Type retention failed");
            model_type=type.object;
        }
        const auto entity=services.entities.Read(owner.entity);
        if(Interrupted(serial,consumer))return nullptr;
        if(!entity.owner_world.read.returned || entity.owner_world.value!=world)throw std::runtime_error("handoff entity world mismatch");
        const auto model=services.entities.Component(owner.entity,model_type);
        if(Interrupted(serial,consumer))return nullptr;
        if(!model.call.returned || !model.object)throw std::runtime_error("handoff own model unavailable");
        return static_cast<CharacterModelComponent*>(model.object);
    }
    void Drain(const WorldFrame& frame) {
        for(auto it=retired.begin();it!=retired.end();) {
            if((*it)->Close(frame).complete)it=retired.erase(it);else ++it;
        }
    }
    bool DetachController(const WorldFrame& frame) {
        if(!controller)return true;
        controller->Close(frame);
        if(!controller->Status().detached)return false;
        retired.push_back(std::move(controller));status.controller_ready=false;
        return true;
    }
    void Fail(const char* message) { status.phase=ModelTransitionPhase::Failed;status.error=message; }
};
ModelTransition::ModelTransition(EntityToken owner,GameWorld* world,
    std::unique_ptr<ModelProfileRequest> request,ModelTransitionServices services)
    :state_(new State(owner,world,std::move(request),std::move(services))){}
ModelTransition::~ModelTransition(){delete state_;}
const ModelTransitionStatus& ModelTransition::Status() const{return state_->status;}
const ModelRequestStatus* ModelTransition::RequestStatus() const{return state_->request?&state_->request->Status():nullptr;}
void ModelTransition::Select(ModelTransitionPlan plan) {
    auto& s=*state_;
    if(s.closing)throw std::logic_error("model transition closing");
    if(plan.destination==ModelDestination::Alternate && !s.request)throw std::logic_error("alternate model requires a captured baseline");
    s.plan=std::move(plan);++s.revision;
    s.status.error.clear();
    s.status.controller_ready=false;s.detach_before_adoption=!s.plan.reload;
    // An existing factory model can be adopted without a replacement request.
    s.status.phase=s.plan.reload?ModelTransitionPhase::Detaching:ModelTransitionPhase::Adopting;
    s.initial_played=false;s.initial_boundary=0;
    s.status.initialization_note.clear();s.status.initial_state={};
}
void ModelTransition::Cancel(){auto& s=*state_;s.closing=true;++s.revision;}
void ModelTransition::Service(const WorldFrame& frame,ManagedObjects& objects,AnimationInputTypes types,
    const ModelTransitionConsumer& consumer) {
    auto& s=*state_;
    if(s.busy || s.closing || s.status.closed || s.Ended() || !s.OwnFrame(frame))return;
    s.busy=true;struct Exit{bool& busy;~Exit(){busy=false;}} exit{s.busy};
    const auto serial=s.revision;
    auto interrupted=[&](){return s.Interrupted(serial,consumer);};
    try {
        s.Drain(frame);
        if(interrupted())return;
        if(s.status.phase==ModelTransitionPhase::Detaching && !s.request) {
            if(!s.DetachController(frame)){s.Fail("own controller override removal unresolved");return;}
            if(interrupted())return;
            s.status.phase=ModelTransitionPhase::Adopting;
        }
        if(s.status.phase==ModelTransitionPhase::Detaching) {
            if(!s.request->Cancel(frame)){s.Fail("model operation cancellation unresolved");return;}
            if(interrupted())return;
            if(!s.DetachController(frame)){s.Fail("own controller override removal unresolved");return;}
            if(interrupted())return;
            const bool detached=!consumer.detach || consumer.detach();
            if(interrupted() || !detached)return;
            auto* model=s.CurrentModel(objects,serial,consumer);
            if(interrupted())return;
            s.request->Submit(frame,model,s.plan.destination);
            if(interrupted())return;
            if(s.request->Status().phase!=ModelRequestPhase::AwaitingModel){s.Fail("official model submission failed");return;}
            s.status.phase=ModelTransitionPhase::Loading;
        }
        if(s.status.phase==ModelTransitionPhase::Loading) {
            auto* model=s.CurrentModel(objects,serial,consumer);
            if(interrupted())return;
            s.request->Observe(frame,model,s.plan.require_animator);
            if(interrupted())return;
            if(s.request->Status().phase==ModelRequestPhase::Failed){s.Fail("official model load observation failed");return;}
            if(s.request->Status().phase!=ModelRequestPhase::ModelReady)return;
            s.status.phase=ModelTransitionPhase::Adopting;
        }
        if(s.status.phase==ModelTransitionPhase::Adopting) {
            if(s.detach_before_adoption) {
                if(!s.DetachController(frame)){s.Fail("own controller override removal unresolved");return;}
                if(interrupted())return;
                s.detach_before_adoption=false;
            }
            const bool adopted=!consumer.adopt || consumer.adopt();
            if(interrupted() || !adopted)return;
            auto* model=s.CurrentModel(objects,serial,consumer);
            if(interrupted())return;
            if(s.plan.controller) {
                if(!s.controller) {
                    s.controller=std::make_unique<ControllerBinding>(s.owner,s.world,s.services.references,s.services.controllers);
                    s.controller->Apply(frame,model,objects,*s.plan.controller);
                }
                if(interrupted())return;
                if(!s.controller->Status().preparation.complete){s.Fail("controller application failed");return;}
            }
            s.status.controller_ready=true;s.status.phase=ModelTransitionPhase::Binding;
        }
        if(s.status.phase==ModelTransitionPhase::Binding) {
            // The original factory model has not replaced its event arrays.
            if(s.request && s.request->Status().phase!=ModelRequestPhase::Idle) {
                auto* model=s.CurrentModel(objects,serial,consumer);
                if(interrupted())return;
                if(!types.events.call.returned || !types.events.object){s.Fail("current event component Type unavailable");return;}
                const auto component=s.services.entities.Component(s.owner.entity,types.events.object);
                if(interrupted())return;
                if(!component.call.returned || !component.object){s.Fail("current event component unavailable");return;}
                s.request->BindEvents(frame,model,component.object,s.services.event_fields);
                s.status.event_value_note=s.request->Status().event_value_note;
                if(interrupted())return;
                if(s.request->Status().phase!=ModelRequestPhase::EventsReady){s.Fail("official event inputs were not adopted");return;}
            }
            if(consumer.events_ready)consumer.events_ready();
            if(interrupted())return;
            s.status.phase=s.plan.initialize?ModelTransitionPhase::Initializing:ModelTransitionPhase::Ready;
        }
        if(s.status.phase==ModelTransitionPhase::Initializing) {
            if(!s.initial_played) {
                if(!consumer.initialize){s.Fail("initial output provider unavailable");return;}
                const auto result=consumer.initialize();
                if(interrupted())return;
                if(result.phase==InitialOutputPhase::Pending)return;
                if(result.phase==InitialOutputPhase::Absent)
                    s.status.initialization_note="configured initial state absent; explicit state selection remains available";
                else if(result.phase==InitialOutputPhase::Returned) {
                    s.initial_played=true;s.initial_hash=result.hash;s.initial_boundary=frame.sequence;
                    return;
                }else {s.Fail("initial state request failed");return;}
            }else {
                // Only a later official LateUpdate delivery can prove output.
                if(!frame.sequence || frame.sequence<=s.initial_boundary)return;
                auto* model=s.CurrentModel(objects,serial,consumer);
                if(interrupted())return;
                const auto current=s.services.models.Read(model);
                if(interrupted())return;
                if(!current.animator.read.returned || !current.animator.value){s.Fail("initial state Animator observation unavailable");return;}
                s.status.initial_state=s.services.animation.CurrentState(current.animator.value);
                if(interrupted())return;
                if(!s.status.initial_state.read.returned){s.Fail("initial Animator output read failed");return;}
                if(s.status.initial_state.short_name_hash!=s.initial_hash)return;
            }
            s.status.phase=ModelTransitionPhase::Ready;
        }
    }catch(const std::exception& e){if(!interrupted())s.Fail(e.what());}
}
bool ModelTransition::Close(const WorldFrame& frame) {
    auto& s=*state_;
    Cancel();
    if(s.status.closed)return true;
    if(s.busy)return false;
    s.busy=true;struct Exit{bool& busy;~Exit(){busy=false;}} exit{s.busy};
    s.status.phase=ModelTransitionPhase::Closing;
    if(!s.Ended() && !s.OwnFrame(frame))return false;
    const bool cancelled=!s.request || s.request->Cancel(frame);
    s.DetachController(frame);s.Drain(frame);
    // Preserve model event values through the official entity exit itself.
    if(s.Ended() && s.request)s.request->Close(frame);
    if(s.request)s.status.event_value_note=s.request->Status().event_value_note;
    bool roots_closed=cancelled || s.Ended();
    if(roots_closed)for(auto it=s.handles.begin();it!=s.handles.end();) {
        if(s.roots.Release(*it).complete){s.roots.Forget(*it);it=s.handles.erase(it);}else {roots_closed=false;++it;}
    }
    s.status.closed=(!s.request || s.request->Status().closed) && !s.controller && s.retired.empty() && roots_closed;
    if(s.status.closed)s.status.phase=ModelTransitionPhase::Closed;
    return s.status.closed;
}
}
