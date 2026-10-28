#pragma once
#include "../Export.hpp"
#include "PresentationPolicy.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <optional>
#include <vector>

namespace hksr::sdk_v1 {
struct CallResult;
struct ManagedObjects;
class RuntimeMetadata;
class EntityOwner;
class CharacterOutfit;
struct EntityOrigin;
struct PresentationBindings;
struct RuntimeCapabilities;
struct ModelRequestServices;
struct ControllerBindingServices;
struct EffectRequestServices;
class ActorControlSession;
class PartnerControlSession;
class ActorAnimationSession;
class NpcCreationSession;
class EntityCreationSession;
class MapNpcInputs;
struct Position3;
struct Rotation4;
class NpcEventSupplement;
class PresentationEffects;
class EffectEntityFollow;
class OwnedTimeline;
class OwnedSoundRequest;
class OwnedMovementRequest;
class OwnedMovementHost;
class OwnedProjectileGroup;
struct ProjectileLaunch;
struct ProjectileArrival;
struct AnimationMovePlan;
struct MovementGeometry;
class EffectSubmissions;
struct EffectSubmission;
struct EffectTriggerExecution;
struct TextResult;
class AnimationEventPolicy;
class AnimationEventConstruction;
struct AnimationEventSource;
struct AnimationTaskConstruction;
struct AnimationEventDefinitions;
class AnimationSequenceExecutions;
struct AnimationSequenceObservation;
class PresentationResources;
class ModelBaselineCapture;
class ModelProfileRequest;
class ModelTransition;
class ModelServices;
class ManagedRoots;
class OfficialCatalog;
class MouseModeLease;
struct WorldMapSnapshot;
struct PlayerPlacement;
template<class T>struct Observed;
struct ProfileDescription;
struct EntityToken;
struct GameWorld;
struct GameEntity;
struct WorldFrame;
// Borrowed references for a ready, initialized model on its owner thread.
// Not a retained model lease; re-read after operations that can replace Art.
struct ActorAnimatorView {
    void* component=nullptr;
    void* animator=nullptr;
    void* controller=nullptr;
};
enum class PresentationIsolationPhase { Unprepared, Prepared, Active, Unavailable, Retired };
struct PresentationIsolationStatus {
    PresentationIsolationPhase phase=PresentationIsolationPhase::Unprepared;
    std::string error;
};
// Per-client engine capability assembly. The metadata provider and objects
// must outlive this context AND all operations obtained from it. Constructor
// only stores native bindings. Prepare resolves declarations on the attached
// preparation thread; all remaining lifecycle methods run on the owner frame.
// Returned factories retain native context state, never a Framework object.
class HKSR_API PresentationContext {
public:
    using Trace=std::function<void(const std::string&)>;
    // Ordinary attached-worker entry: SDK resolves its own binding assembly.
    // The explicit constructor below is for low-level composition/test providers.
    static std::unique_ptr<PresentationContext> Create(RuntimeMetadata&,RuntimeCapabilities,
        PresentationIsolationPolicy,Trace={});
    PresentationContext(ManagedObjects&,PresentationBindings);
    ~PresentationContext(); // native destruction only; call Close after consumers drain
    PresentationContext(const PresentationContext&)=delete;
    PresentationContext& operator=(const PresentationContext&)=delete;
    void Prepare(RuntimeMetadata&,RuntimeCapabilities,PresentationIsolationPolicy,Trace);
    PresentationIsolationStatus IsolationStatus() const;
    const PresentationBindings& Bindings() const;
    const PresentationResources& Resources() const;
    std::shared_ptr<const ModelServices> Models() const;
    std::unique_ptr<ManagedRoots> CreateRoots() const;
    WorldMapSnapshot ReadMap(const WorldFrame&) const;
    PlayerPlacement ReadPlayerPlacement(const WorldFrame&) const;
    PlayerPlacement ReadEntityPlacement(EntityToken,GameWorld*) const;
    Observed<Position3> ReadLogicalPosition(EntityToken,GameWorld*) const;
    // No component is created. Nullopt means CharacterData is absent; errors
    // remain errors rather than an implicit zero-radius fallback.
    std::optional<MovementGeometry> ReadMovementGeometry(EntityToken,GameWorld*) const;
    const std::vector<std::string>& BindingErrors() const;
    // Prepared runtime only. Catalog construction and mouse observation retain
    // the same preparation-thread requirements as their explicit providers.
    std::unique_ptr<OfficialCatalog> CreateCatalog() const;
    std::unique_ptr<MouseModeLease> CreateMouseMode() const;
    // Nullopt while the model/controller is unavailable or its entity ended.
    std::optional<ActorAnimatorView> ReadAnimator(EntityToken) const;
    // Current event consumer, not the last observed construction receiver.
    // Borrowed on the owner thread; null while unavailable or being replaced.
    void* ReadEventReceiver(EntityToken,GameWorld*) const;
    // Throws when the entity is not loaded in the requested world, has ended,
    // or has no FreeStyle component. The returned component is borrowed.
    void* NpcMotionComponent(EntityToken,GameWorld*) const;
    const EntityOwner& Owner() const;
    using NpcFactory=std::function<std::unique_ptr<NpcCreationSession>()>;
    NpcFactory Npcs() const;
    using EntityFactory=std::function<std::unique_ptr<EntityCreationSession>()>;
    EntityFactory Entities() const;
    // Official auxiliary actors. Uses the same birth provenance and event
    // isolation as other owned entities, without Story/NPC initialization.
    EntityFactory Partners() const;
    std::unique_ptr<PartnerControlSession> PartnerControls(EntityToken partner,EntityToken summoner,GameWorld*) const;
    using NpcInputFactory=std::function<std::unique_ptr<MapNpcInputs>(std::uint32_t,const char*,Position3,Rotation4)>;
    NpcInputFactory NpcInputs() const;
    using NpcEventFactory=std::function<std::unique_ptr<NpcEventSupplement>(NpcCreationSession&,GameWorld*)>;
    NpcEventFactory NpcEvents() const;
    using EntityEventFactory=std::function<std::unique_ptr<NpcEventSupplement>(EntityCreationSession&,GameWorld*)>;
    EntityEventFactory EntityEvents() const;
    using AnimationFactory=std::function<std::unique_ptr<ActorAnimationSession>(EntityToken)>;
    AnimationFactory Animations() const;
    std::function<void(EntityOrigin)> BirthObserver() const;
    std::function<std::unique_ptr<CharacterOutfit>()> Outfits() const;
    ModelRequestServices ModelRequests() const;
    using BaselineFactory=std::function<std::unique_ptr<ModelBaselineCapture>(EntityToken,GameWorld*,const ProfileDescription&)>;
    using ModelTransitionFactory=std::function<std::unique_ptr<ModelTransition>(EntityToken,GameWorld*,std::unique_ptr<ModelProfileRequest>)>;
    BaselineFactory Baselines() const;
    ModelTransitionFactory ModelTransitions() const;
    ControllerBindingServices Controllers() const;
    std::optional<EffectRequestServices> Effects() const;
    std::shared_ptr<const PresentationEffects> EffectServices() const;
    // Caller controls target lifetimes and closes this adapter on the engine
    // thread. Binding targets is not permission to dispose/move other entities.
    std::unique_ptr<EffectEntityFollow> CreateEffectEntityFollow() const;
    // Engine-thread Common playback. The caller services the request through
    // pool return, including after the actor/action has ended.
    std::unique_ptr<OwnedTimeline> CreateTimeline(EntityToken,GameWorld*) const;
    // Owner-frame request on this client's current model Art. The caller owns
    // cancellation and services the receipt to closure before releasing it.
    // No implicit main-listener fallback or ability/task execution.
    std::unique_ptr<OwnedSoundRequest> StartSound(EntityToken,GameWorld*,const std::string& event) const;
    // Explicit resolved endpoints, current owned model and official layer-zero
    // Animator state. Captures the live origin and computes the state key;
    // does not play the animation. Cancel/drain before changing the model.
    std::unique_ptr<OwnedMovementRequest> StartMovement(EntityToken,GameWorld*,
        const std::string& state,AnimationMovePlan) const;
    // Exclusive animation/effect movement on an owned actor. A host may be
    // waiting for already-submitted Adventure motion; check Ready before use.
    // Close after detaching all request-owned motion, before model handoff.
    std::shared_ptr<OwnedMovementHost> AcquireMovementHost(EntityToken,GameWorld*) const;
    // Private emission group; Prepare/Fire/Service are explicit. Close it before
    // changing either captured model. No task or ability registration occurs.
    std::unique_ptr<OwnedProjectileGroup> CreateProjectiles(EntityToken,GameWorld*,
        std::function<void(const ProjectileArrival&)> arrival) const;
    ProjectileLaunch CaptureProjectileTarget(EntityToken,GameWorld*) const;
    TextResult ReadEffectTriggerPath(const EffectTriggerExecution&) const;
    // Construct during preparation; activate and stop on the owner thread.
    // Receives only this client's entity generations, including creation-time
    // and retained retirement provenance. Callbacks must not service removals.
    std::unique_ptr<EffectSubmissions> CreateEffectObserver(
        std::function<void(const EffectSubmission&)> before,
        std::function<void(const EffectSubmission&,bool)> after) const;
    // Factories retain native context state. Sessions still require Close on
    // their official owner frame before the context itself is closed.
    using ControlFactory=std::function<std::unique_ptr<ActorControlSession>(EntityToken,GameWorld*)>;
    ControlFactory Controls() const;
    // Resolve only this client's owned model and submit a model-centred turn.
    // The point is explicit; no battle target selector or team propagation runs.
    // Native return is submission, not completion of the smoothed rotation.
    CallResult FaceEntity(EntityToken,GameWorld*,Position3 target,float ratio=1,float angle_offset=0,float duration=-1) const;
    using PlayerResolver=std::function<GameEntity*(const WorldFrame&)>;
    PlayerResolver Player() const;
    std::unique_ptr<AnimationEventPolicy> CreateAnimationPolicy(AnimationTaskPolicy) const;
    std::unique_ptr<AnimationEventConstruction> CreateEventConstructionObserver(
        std::function<void(const AnimationEventSource&)> before,
        std::function<void(const AnimationEventSource&,const AnimationTaskConstruction&)> tasks,
        std::function<void(const AnimationEventSource&,bool)> after) const;
    std::unique_ptr<AnimationSequenceExecutions> CreateSequenceObserver(
        std::function<void(const AnimationSequenceObservation&)>) const;
    AnimationEventDefinitions ReadEventDefinitions(const AnimationEventSource&) const;
    void Activate();
    // Owner-thread visibility preference for this client's effects. Does not
    // grant main-camera control or environment-write permissions. Existing
    // effects take the value on their next official visibility evaluation.
    void SetCameraPresentations(bool enabled);
    void Collect();
    // Call only after owned requests, registrations and entities are closed.
    // Continues cleanup without repeating uncertain reference decrements;
    // retires isolation only when cleanup is known complete.
    // Not permission to hot-unload the SDK or its process-resident hooks.
    bool Close();
private:
    struct State;
    struct Storage;
    Storage* storage_;
};
}
