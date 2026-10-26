#include <hksr/animation/ActorAnimationSession.hpp>
#include <hksr/animation/AnimationResources.hpp>
#include <hksr/presentation/PresentationBindings.hpp>
#include <hksr/entity/LifecycleHooks.hpp>

namespace hksr::sdk_v1 {
struct ActorAnimationSession::State {
    EntityToken owner;
    EntityAccess entities;
    ModelAccess models;
    AnimationControl control;
    ProfileInputs inputs;
    AnimationResources resources;
    struct Spatial {
        HitGeometry geometry;HitDirection direction;WorldPlacement placement;SceneObjects scene;
        explicit Spatial(const PresentationBindings& b):geometry(b.hit_geometry),direction(b.hit_direction),placement(b.placement),scene(b.scene){}
    };
    std::unique_ptr<Spatial> spatial;
    bool Live() const {
        return !TrackedEntities().IsDisposed(owner) && !TrackedEntities().IsRetiring(owner) &&
            TrackedEntities().Current(owner.entity).generation==owner.generation;
    }
    State(EntityToken token,ReferenceMethods refs,EntityAccess entity,ModelAccess model,AnimationControl animation,ProfileInputs profile)
        :owner(token),entities(std::move(entity)),models(std::move(model)),control(std::move(animation)),inputs(std::move(profile)),
         resources(token,refs,entities){}
};
ActorAnimationSession::ActorAnimationSession(EntityToken owner,const PresentationBindings& bindings)
    :ActorAnimationSession(owner,bindings.references,EntityAccess(bindings.entity),ModelAccess(bindings.model),
        AnimationControl(bindings.animation),ProfileInputs(bindings.profile_inputs)) {state_->spatial=std::make_unique<State::Spatial>(bindings);}
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
Observed<float> ActorAnimationSession::ReadHitAngle(Position3 attacker,const HitShapeParameters& shape,const std::string& attachment,
    float horizontal,float vertical,ManagedObjects& objects) {
    auto& s=*state_;if(!s.spatial || !s.Live())return {};
    auto& reads=*s.spatial;
    const auto model=s.resources.Component(AnimationComponent::Model,objects);
    if(!s.Live() || !model.call.returned || !model.object)return {};
    const auto snapshot=s.models.Read(static_cast<CharacterModelComponent*>(model.object));
    if(!s.Live() || !snapshot.changing.read.returned || snapshot.changing.value || !snapshot.animator.read.returned || !snapshot.animator.value)return {};
    const auto transform=s.resources.Component(AnimationComponent::Transform,objects);
    if(!s.Live() || !transform.call.returned || !transform.object)return {};
    void* anchor=nullptr;
    if(!attachment.empty()) {
        auto* name=s.resources.Text(attachment,objects);if(!s.Live())return {};
        const auto result=reads.geometry.Anchor(model.object,name);
        if(!s.Live() || !result.call.returned)return {};
        anchor=result.object;
    }
    const auto geometry=reads.geometry.Read(transform.object,anchor,shape);
    if(!s.Live() || !geometry.read.returned)return {};
    const auto world=reads.direction.WorldDirection(attacker,geometry.value.center,horizontal,vertical);
    if(!s.Live() || !world.read.returned)return {};
    const auto mirrored=reads.direction.Mirrored(model.object);
    if(!mirrored.read.returned)return {};
    const auto go=reads.placement.Root(s.owner.entity);
    if(!s.Live() || !go.call.returned || !go.object)return {};
    const auto root=reads.scene.Transform(go.object);
    if(!s.Live() || !root.call.returned || !root.object)return {};
    const auto angle=reads.direction.Read(root.object,world.value,mirrored.value);
    if(!s.Live())return {};
    const auto current_component=s.resources.Component(AnimationComponent::Model,objects);
    if(!s.Live() || !current_component.call.returned || current_component.object!=model.object)return {};
    const auto current=s.models.Read(static_cast<CharacterModelComponent*>(model.object));
    return s.Live() && current.animator.read.returned && current.animator.value==snapshot.animator.value &&
        current.changing.read.returned && !current.changing.value?angle:Observed<float>{};
}
bool ActorAnimationSession::Close(){return state_->resources.Close();}
}
