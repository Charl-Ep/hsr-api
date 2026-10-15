#include <hksr/animation/ActorAnimationSession.hpp>
#include <hksr/animation/AnimationResources.hpp>
#include <hksr/presentation/PresentationBindings.hpp>

namespace hksr::sdk_v1 {
struct ActorAnimationSession::State {
    EntityToken owner;
    EntityAccess entities;
    ModelAccess models;
    AnimationControl control;
    ProfileInputs inputs;
    AnimationResources resources;
    State(EntityToken token,ReferenceMethods refs,EntityAccess entity,ModelAccess model,AnimationControl animation,ProfileInputs profile)
        :owner(token),entities(std::move(entity)),models(std::move(model)),control(std::move(animation)),inputs(std::move(profile)),
         resources(token,refs,entities){}
};
ActorAnimationSession::ActorAnimationSession(EntityToken owner,const PresentationBindings& bindings)
    :ActorAnimationSession(owner,bindings.references,EntityAccess(bindings.entity),ModelAccess(bindings.model),
        AnimationControl(bindings.animation),ProfileInputs(bindings.profile_inputs)){}
ActorAnimationSession::ActorAnimationSession(EntityToken owner,ReferenceMethods refs,EntityAccess entities,
    ModelAccess models,AnimationControl control,ProfileInputs inputs)
    :state_(new State(owner,refs,std::move(entities),std::move(models),std::move(control),std::move(inputs))){}
ActorAnimationSession::~ActorAnimationSession(){delete state_;}
EntityToken ActorAnimationSession::Owner() const{return state_->owner;}
const EntityAccess& ActorAnimationSession::Entities() const{return state_->entities;}
const ModelAccess& ActorAnimationSession::Models() const{return state_->models;}
const AnimationControl& ActorAnimationSession::Control() const{return state_->control;}
const ProfileInputs& ActorAnimationSession::Inputs() const{return state_->inputs;}
AnimationResources& ActorAnimationSession::Resources(){return state_->resources;}
bool ActorAnimationSession::Close(){return state_->resources.Close();}
}
