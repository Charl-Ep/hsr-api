#pragma once
#include "../Export.hpp"
#include "../presentation/hits/HitGeometry.hpp"

namespace hksr::sdk_v1 {
struct EntityToken;
struct PresentationBindings;
struct ReferenceMethods;
class EntityAccess;
class ModelAccess;
class AnimationControl;
class ProfileInputs;
class AnimationResources;

// SDK-owned call bindings and managed inputs for one entity epoch. Construction
// is native-only; Close releases managed inputs on the owning engine thread.
// Does not select, play, advance or restore an animation automatically.
class HKSR_API ActorAnimationSession {
public:
    ActorAnimationSession(EntityToken,const PresentationBindings&); // low-level providers
    ActorAnimationSession(EntityToken,ReferenceMethods,EntityAccess,ModelAccess,AnimationControl,ProfileInputs);
    ~ActorAnimationSession(); // no engine calls; drain Close before destruction
    ActorAnimationSession(const ActorAnimationSession&)=delete;
    ActorAnimationSession& operator=(const ActorAnimationSession&)=delete;
    EntityToken Owner() const;
    const EntityAccess& Entities() const;
    const ModelAccess& Models() const;
    const AnimationControl& Control() const;
    const ProfileInputs& Inputs() const;
    AnimationResources& Resources();
    Observed<float> ReadHitAngle(Position3 attacker,const HitShapeParameters&,const std::string& attach_point,
        float horizontal_degrees,float vertical_degrees,ManagedObjects&);
    Observed<HitEffectPlacement> ReadHitEffectPlacement(Position3 attacker,const HitShapeParameters&,
        const std::string& attach_point,float height,float angle_degrees,const std::optional<Position3>& offset_override,
        bool offset_by_scale,float delay,ManagedObjects&);
    bool Close();
private:
    struct State;
    State* state_;
};
}
