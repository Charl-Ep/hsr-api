#pragma once
#include "../Export.hpp"
#include "../presentation/hits/HitGeometry.hpp"
#include "AnimationParameter.hpp"
#include "HitDirection.hpp"
#include "../world/WorldPlacement.hpp"
#include "../unity/SceneObjects.hpp"
#include <functional>
#include <optional>

namespace hksr::api {
struct EntityToken;
struct ReferenceMethods;
class EntityAccess;
class ModelAccess;
class AnimationControl;
class ProfileInputs;
class AnimationResources;
class PresentationContext;

// Optional actor-bound spatial reads. Each query uses only its own methods;
// missing direction bindings do not disable hit-effect placement.
struct ActorHitGeometryBindings {
    HitGeometryMethods geometry;
    HitDirectionMethods direction;
    WorldPlacementMethods placement;
    SceneObjectMethods scene;
};

// SDK-owned call bindings and managed inputs for one entity epoch. Construction
// is native-only; Close releases managed inputs on the owning engine thread.
// Does not select, play, advance or restore an animation automatically.
class HKSR_API ActorAnimationSession {
  public:
    ActorAnimationSession(EntityToken, ReferenceMethods, EntityAccess, ModelAccess, AnimationControl, ProfileInputs,
                          std::optional<ActorHitGeometryBindings> spatial = {});
    ~ActorAnimationSession(); // no engine calls; drain Close before destruction
    ActorAnimationSession(const ActorAnimationSession&) = delete;
    ActorAnimationSession& operator=(const ActorAnimationSession&) = delete;
    EntityToken Owner() const;
    const EntityAccess& Entities() const;
    const ModelAccess& Models() const;
    const AnimationControl& Control() const;
    const ProfileInputs& Inputs() const;
    AnimationResources& Resources();
    // Available on sessions returned by a prepared PresentationContext. Engine
    // thread only; binds this owned actor and its current model for the call.
    AnimationParameterResult SetParameter(GameWorld*, const AnimationParameter&,
                                          AnimationParameterPropagation = AnimationParameterPropagation::BodyOnly);
    // Unavailable without spatial bindings, after Close, or when this entity/model
    // ends during the query. No binding means no component lookup or allocation.
    Observed<float> ReadHitAngle(Position3 attacker, const HitShapeParameters&, const std::string& attach_point,
                                 float horizontal_degrees, float vertical_degrees, ManagedObjects&);
    Observed<HitEffectPlacement> ReadHitEffectPlacement(Position3 attacker, const HitShapeParameters&,
                                                        const std::string& attach_point, float height,
                                                        float angle_degrees,
                                                        const std::optional<Position3>& offset_override,
                                                        bool offset_by_scale, float delay, ManagedObjects&);
    bool Close();

  private:
    friend class PresentationContext;
    using ParameterWriter =
        std::function<AnimationParameterResult(GameWorld*, const AnimationParameter&, AnimationParameterPropagation)>;
    void BindParameters(ParameterWriter);
    struct State;
    State* state_;
};
} // namespace hksr::api
