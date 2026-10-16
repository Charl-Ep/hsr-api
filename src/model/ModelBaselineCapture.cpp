#include <hksr/model/ModelBaselineCapture.hpp>
#include <hksr/runtime/OperationTrace.hpp>
#include <hksr/model/ModelVisibilityLease.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct ModelBaselineCapture::State {
    EntityToken owner;
    GameWorld* world;
    ReferenceMethods references;
    EntityAccess entities;
    ModelRequestServices services;
    ProfileEventFields fields;
    ProfileDescription target;
    ManagedRoots roots;
    std::vector<ManagedRootId> handles;
    std::unique_ptr<ModelProfileInputs> inputs;
    std::unique_ptr<ModelVisibilityLease> visibility;
    void* model_type=nullptr;
    bool cancelled=false,busy=false;
    BaselineCaptureStatus status;
    State(EntityToken o,GameWorld* w,ReferenceMethods refs,EntityAccess e,ModelRequestServices s,
        ProfileEventFields f,ProfileDescription t)
        :owner(o),world(w),references(refs),entities(std::move(e)),services(std::move(s)),fields(std::move(f)),
        target(std::move(t)),roots(refs){}
    bool Interrupted() {
        if(cancelled){status.phase=BaselineCapturePhase::Cancelled;return true;}
        if(TrackedEntities().IsDisposed(owner) || TrackedEntities().Current(owner.entity).generation!=owner.generation) {
            status.phase=BaselineCapturePhase::OwnerEnded;return true;
        }
        return false;
    }
};
ModelBaselineCapture::ModelBaselineCapture(EntityToken owner,GameWorld* world,ReferenceMethods refs,
    EntityAccess entities,ModelRequestServices services,ProfileEventFields fields,ProfileDescription target)
    :state_(new State(owner,world,refs,std::move(entities),std::move(services),std::move(fields),std::move(target))){}
ModelBaselineCapture::~ModelBaselineCapture(){delete state_;}
void ModelBaselineCapture::SetVisibilityLease(std::unique_ptr<ModelVisibilityLease> visibility) {
    if(state_->status.attempted || state_->visibility)throw std::logic_error("baseline visibility lease already configured");
    state_->visibility=std::move(visibility);
}
const BaselineCaptureStatus& ModelBaselineCapture::Status() const{return state_->status;}
void ModelBaselineCapture::Cancel(){state_->cancelled=true;}
void ModelBaselineCapture::Service(const WorldFrame& frame,ManagedObjects& objects,
    const ManagedArrays& arrays,AnimationInputTypes types) {
    OperationSpan timing("sdk.model.baseline");
    auto& s=*state_;
    if(s.busy || s.status.closed || s.status.phase!=BaselineCapturePhase::Waiting)return;
    s.busy=true;struct Exit{bool& busy;~Exit(){busy=false;}} exit{s.busy};
    if(s.Interrupted())return;
    if(frame.world!=s.world || !frame.disposing.read.returned || frame.disposing.value)return;
    try {
        if(!s.model_type) {
            const auto type=objects.ManagedType("RPG.GameCore","CharacterModelComponent");
            if(s.Interrupted())return;
            if(!type.call.returned || !type.object)throw std::runtime_error("baseline model Type unavailable");
            const auto id=s.roots.Keep(type.object);s.handles.push_back(id);
            if(s.Interrupted())return;
            if(!s.roots.Status(id).ready)throw std::runtime_error("baseline model Type retention failed");
            s.model_type=type.object;
        }
        const auto entity=s.entities.Read(s.owner.entity);
        if(s.Interrupted())return;
        if(!entity.owner_world.read.returned || entity.owner_world.value!=s.world)throw std::runtime_error("baseline entity world mismatch");
        const auto component=s.entities.Component(s.owner.entity,s.model_type);
        if(s.Interrupted())return;
        if(!component.call.returned)throw std::runtime_error("baseline model component read failed");
        if(!component.object){s.status.waiting="official model component not yet available";return;}
        const auto snapshot=s.services.model.Read(static_cast<CharacterModelComponent*>(component.object));
        if(s.Interrupted())return;
        if(!s.services.completion)throw std::runtime_error("baseline load-state contract unavailable");
        const auto complete=s.services.completion(snapshot);
        if(!complete.read.returned)throw std::runtime_error("baseline load-state read failed");
        if(!complete.value){s.status.waiting="official baseline model is loading";return;}
        if(!types.events.call.returned || !types.events.object)throw std::runtime_error("baseline event component Type unavailable");
        const auto event=s.entities.Component(s.owner.entity,types.events.object);
        if(s.Interrupted())return;
        if(!event.call.returned)throw std::runtime_error("baseline event component read failed");
        if(!event.object){s.status.waiting="official event component not yet available";return;}
        const auto consumer=s.services.events.Consumer(event.object);
        if(s.Interrupted())return;
        if(!consumer.call.returned)throw std::runtime_error("baseline event consumer read failed");
        if(!consumer.object){s.status.waiting="official event consumer not yet available";return;}
        s.status.attempted=true;
        s.inputs=std::make_unique<ModelProfileInputs>(s.references);
        s.inputs->Prepare(objects,arrays,s.services.inputs,s.fields,component.object,consumer.object,s.target);
        if(s.Interrupted())return;
        if(!s.inputs->Status().prepared) {
            s.status.errors=s.inputs->Status().errors;
            throw std::runtime_error("model baseline inputs unavailable");
        }
        s.status.phase=BaselineCapturePhase::Ready;s.status.waiting.clear();
    }catch(const std::exception& e) {
        s.status.attempted=true;s.status.errors.push_back(e.what());
        if(!s.Interrupted())s.status.phase=BaselineCapturePhase::Failed;
    }
}
std::unique_ptr<ModelProfileRequest> ModelBaselineCapture::TakeRequest() {
    auto& s=*state_;
    if(s.status.closed || s.Interrupted() || s.status.phase!=BaselineCapturePhase::Ready)
        throw std::logic_error("baseline request is not ready for transfer");
    auto request=std::make_unique<ModelProfileRequest>(s.owner,s.world,std::move(s.inputs),s.references,s.services);
    if(s.visibility)request->SetVisibilityLease(std::move(s.visibility));
    s.status.phase=BaselineCapturePhase::Transferred;
    return request;
}
bool ModelBaselineCapture::Close() {
    auto& s=*state_;
    s.cancelled=true;
    // A synchronous callback may request Close while Prepare still uses its
    // inputs. Leave those roots intact until Service unwinds.
    if(s.busy)return false;
    bool closed=true;
    if(s.visibility && !s.visibility->Close())closed=false;
    if(s.inputs) {
        if(s.inputs->Close().complete)s.inputs.reset();else closed=false;
    }
    for(auto it=s.handles.begin();it!=s.handles.end();) {
        if(s.roots.Release(*it).complete){s.roots.Forget(*it);it=s.handles.erase(it);}else {closed=false;++it;}
    }
    s.status.closed=closed;
    return closed;
}
}
