#include <hksr/animation/ActorAnimationSession.hpp>
#include <hksr/model/ModelAccess.hpp>
#include <hksr/animation/AnimationResources.hpp>
#include <hksr/animation/AnimationControl.hpp>
#include <hksr/model/ProfileInputs.hpp>
#include <hksr/entity/EntityAccess.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <utility>

namespace hksr::api {
struct ActorAnimationSession::State {
    EntityToken owner;
    EntityAccess entities;
    ModelAccess models;
    AnimationControl control;
    ProfileInputs inputs;
    AnimationResources resources;
    ParameterWriter parameters;
    bool closed = false;
    struct Spatial {
        HitGeometry geometry;
        HitDirection direction;
        WorldPlacement placement;
        SceneObjects scene;
        explicit Spatial(ActorHitGeometryBindings b)
            : geometry(std::move(b.geometry)), direction(std::move(b.direction)), placement(std::move(b.placement)),
              scene(std::move(b.scene)) {}
    };
    std::optional<Spatial> spatial;
    bool Live() const {
        return !closed && !TrackedEntities().IsDisposed(owner) && !TrackedEntities().IsRetiring(owner) &&
               TrackedEntities().Current(owner.entity).generation == owner.generation;
    }
    struct SpatialView {
        void* model = nullptr;
        void* animator = nullptr;
        void* logical = nullptr;
        void* root = nullptr;
        HitShapeSnapshot shape;
    };
    std::optional<SpatialView> ReadSpatial(const HitShapeParameters& shape, const std::string& attachment,
                                           ManagedObjects& objects) {
        if (!spatial || !Live())
            return {};
        auto& reads = *spatial;
        SpatialView out;
        const auto model = resources.Component(AnimationComponent::Model, objects);
        if (!Live() || !model.call.returned || !model.object)
            return {};
        const auto snapshot = models.Read(static_cast<CharacterModelComponent*>(model.object));
        if (!Live() || !snapshot.changing.read.returned || snapshot.changing.value ||
            !snapshot.animator.read.returned || !snapshot.animator.value)
            return {};
        out.model = model.object;
        out.animator = snapshot.animator.value;
        const auto transform = resources.Component(AnimationComponent::Transform, objects);
        if (!Live() || !transform.call.returned || !transform.object)
            return {};
        out.logical = transform.object;
        void* anchor = nullptr;
        if (!attachment.empty()) {
            auto* name = resources.Text(attachment, objects);
            if (!Live())
                return {};
            const auto result = reads.geometry.Anchor(model.object, name);
            if (!Live() || !result.call.returned)
                return {};
            anchor = result.object;
        }
        const auto geometry = reads.geometry.Read(transform.object, anchor, shape);
        if (!Live() || !geometry.read.returned)
            return {};
        out.shape = geometry.value;
        const auto go = reads.placement.Root(owner.entity);
        if (!Live() || !go.call.returned || !go.object)
            return {};
        const auto root = reads.scene.Transform(go.object);
        if (!Live() || !root.call.returned || !root.object)
            return {};
        out.root = root.object;
        return out;
    }
    bool SameSpatial(const SpatialView& view, ManagedObjects& objects) {
        if (!Live())
            return false;
        const auto component = resources.Component(AnimationComponent::Model, objects);
        if (!Live() || !component.call.returned || component.object != view.model)
            return false;
        const auto current = models.Read(static_cast<CharacterModelComponent*>(view.model));
        return Live() && current.animator.read.returned && current.animator.value == view.animator &&
               current.changing.read.returned && !current.changing.value;
    }
    State(EntityToken token, ReferenceMethods refs, EntityAccess entity, ModelAccess model, AnimationControl animation,
          ProfileInputs profile, std::optional<ActorHitGeometryBindings> hit_geometry)
        : owner(token), entities(std::move(entity)), models(std::move(model)), control(std::move(animation)),
          inputs(std::move(profile)), resources(token, refs, entities) {
        if (hit_geometry)
            spatial.emplace(std::move(*hit_geometry));
    }
};
ActorAnimationSession::ActorAnimationSession(EntityToken owner, ReferenceMethods refs, EntityAccess entities,
                                             ModelAccess models, AnimationControl control, ProfileInputs inputs,
                                             std::optional<ActorHitGeometryBindings> spatial)
    : state_(new State(owner, refs, std::move(entities), std::move(models), std::move(control), std::move(inputs),
                       std::move(spatial))) {}
ActorAnimationSession::~ActorAnimationSession() {
    delete state_;
}
EntityToken ActorAnimationSession::Owner() const {
    return state_->owner;
}
const EntityAccess& ActorAnimationSession::Entities() const {
    return state_->entities;
}
const ModelAccess& ActorAnimationSession::Models() const {
    return state_->models;
}
const AnimationControl& ActorAnimationSession::Control() const {
    return state_->control;
}
const ProfileInputs& ActorAnimationSession::Inputs() const {
    return state_->inputs;
}
AnimationResources& ActorAnimationSession::Resources() {
    return state_->resources;
}
void ActorAnimationSession::BindParameters(ParameterWriter writer) {
    state_->parameters = std::move(writer);
}
AnimationParameterResult ActorAnimationSession::SetParameter(GameWorld* world, const AnimationParameter& value,
                                                             AnimationParameterPropagation propagation) {
    if (!state_->Live() || !state_->parameters)
        return {};
    return state_->parameters(world, value, propagation);
}
Observed<float> ActorAnimationSession::ReadHitAngle(Position3 attacker, const HitShapeParameters& shape,
                                                    const std::string& attachment, float horizontal, float vertical,
                                                    ManagedObjects& objects) {
    auto& s = *state_;
    try {
        const auto view = s.ReadSpatial(shape, attachment, objects);
        if (!view)
            return {};
        auto& reads = *s.spatial;
        const auto world = reads.direction.WorldDirection(attacker, view->shape.center, horizontal, vertical);
        if (!s.Live() || !world.read.returned)
            return {};
        const auto mirrored = reads.direction.Mirrored(view->model);
        if (!mirrored.read.returned)
            return {};
        const auto angle = reads.direction.Read(view->root, world.value, mirrored.value);
        return s.SameSpatial(*view, objects) ? angle : Observed<float>{};
    } catch (...) {
        if (!s.Live())
            return {};
        throw;
    }
}
Observed<HitEffectPlacement> ActorAnimationSession::ReadHitEffectPlacement(
    Position3 attacker, const HitShapeParameters& shape, const std::string& attachment, float height, float angle,
    const std::optional<Position3>& offset, bool scaled, float delay, ManagedObjects& objects) {
    auto& s = *state_;
    try {
        const auto view = s.ReadSpatial(shape, attachment, objects);
        if (!view)
            return {};
        const auto result = s.spatial->geometry.EffectPlacement(view->logical, view->root, view->shape, attacker,
                                                                height, angle, offset, scaled, delay);
        return s.SameSpatial(*view, objects) ? result : Observed<HitEffectPlacement>{};
    } catch (...) {
        if (!s.Live())
            return {};
        throw;
    }
}
bool ActorAnimationSession::Close() {
    state_->closed = true;
    return state_->resources.Close();
}
} // namespace hksr::api
