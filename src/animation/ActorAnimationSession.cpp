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
    struct SpatialView {void* model=nullptr;void* animator=nullptr;void* logical=nullptr;void* root=nullptr;HitShapeSnapshot shape;};
    std::optional<SpatialView> ReadSpatial(const HitShapeParameters& shape,const std::string& attachment,ManagedObjects& objects) {
        if(!spatial || !Live())return {};
        auto& reads=*spatial;SpatialView out;
        const auto model=resources.Component(AnimationComponent::Model,objects);
        if(!Live() || !model.call.returned || !model.object)return {};
        const auto snapshot=models.Read(static_cast<CharacterModelComponent*>(model.object));
        if(!Live() || !snapshot.changing.read.returned || snapshot.changing.value || !snapshot.animator.read.returned || !snapshot.animator.value)return {};
        out.model=model.object;out.animator=snapshot.animator.value;
        const auto transform=resources.Component(AnimationComponent::Transform,objects);
        if(!Live() || !transform.call.returned || !transform.object)return {};
        out.logical=transform.object;void* anchor=nullptr;
        if(!attachment.empty()) {
            auto* name=resources.Text(attachment,objects);if(!Live())return {};
            const auto result=reads.geometry.Anchor(model.object,name);
            if(!Live() || !result.call.returned)return {};
            anchor=result.object;
        }
        const auto geometry=reads.geometry.Read(transform.object,anchor,shape);
        if(!Live() || !geometry.read.returned)return {};
        out.shape=geometry.value;
        const auto go=reads.placement.Root(owner.entity);
        if(!Live() || !go.call.returned || !go.object)return {};
        const auto root=reads.scene.Transform(go.object);
        if(!Live() || !root.call.returned || !root.object)return {};
        out.root=root.object;return out;
    }
    bool SameSpatial(const SpatialView& view,ManagedObjects& objects) {
        if(!Live())return false;
        const auto component=resources.Component(AnimationComponent::Model,objects);
        if(!Live() || !component.call.returned || component.object!=view.model)return false;
        const auto current=models.Read(static_cast<CharacterModelComponent*>(view.model));
        return Live() && current.animator.read.returned && current.animator.value==view.animator && current.changing.read.returned && !current.changing.value;
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
    auto& s=*state_;const auto view=s.ReadSpatial(shape,attachment,objects);if(!view)return {};
    auto& reads=*s.spatial;
    const auto world=reads.direction.WorldDirection(attacker,view->shape.center,horizontal,vertical);
    if(!s.Live() || !world.read.returned)return {};
    const auto mirrored=reads.direction.Mirrored(view->model);
    if(!mirrored.read.returned)return {};
    const auto angle=reads.direction.Read(view->root,world.value,mirrored.value);
    return s.SameSpatial(*view,objects)?angle:Observed<float>{};
}
Observed<HitEffectPlacement> ActorAnimationSession::ReadHitEffectPlacement(Position3 attacker,const HitShapeParameters& shape,
    const std::string& attachment,float height,float angle,const std::optional<Position3>& offset,bool scaled,float delay,ManagedObjects& objects) {
    auto& s=*state_;const auto view=s.ReadSpatial(shape,attachment,objects);if(!view)return {};
    const auto result=s.spatial->geometry.EffectPlacement(view->logical,view->root,view->shape,attacker,height,angle,offset,scaled,delay);
    return s.SameSpatial(*view,objects)?result:Observed<HitEffectPlacement>{};
}
bool ActorAnimationSession::Close(){return state_->resources.Close();}
}
