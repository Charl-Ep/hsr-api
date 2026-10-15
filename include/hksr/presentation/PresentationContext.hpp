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
class ActorAnimationSession;
class NpcCreationSession;
class EntityCreationSession;
class MapNpcInputs;
struct Position3;
struct Rotation4;
class NpcEventSupplement;
class PresentationEffects;
class AnimationEventPolicy;
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
    const std::vector<std::string>& BindingErrors() const;
    // Prepared runtime only. Catalog construction and mouse observation retain
    // the same preparation-thread requirements as their explicit providers.
    std::unique_ptr<OfficialCatalog> CreateCatalog() const;
    std::unique_ptr<MouseModeLease> CreateMouseMode() const;
    // Nullopt while the model/controller is unavailable or its entity ended.
    std::optional<ActorAnimatorView> ReadAnimator(EntityToken) const;
    // Throws when the entity is not loaded in the requested world, has ended,
    // or has no FreeStyle component. The returned component is borrowed.
    void* NpcMotionComponent(EntityToken,GameWorld*) const;
    const EntityOwner& Owner() const;
    using NpcFactory=std::function<std::unique_ptr<NpcCreationSession>()>;
    NpcFactory Npcs() const;
    using EntityFactory=std::function<std::unique_ptr<EntityCreationSession>()>;
    EntityFactory Entities() const;
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
    // Factories retain native context state. Sessions still require Close on
    // their official owner frame before the context itself is closed.
    using ControlFactory=std::function<std::unique_ptr<ActorControlSession>(EntityToken,GameWorld*)>;
    ControlFactory Controls() const;
    using PlayerResolver=std::function<GameEntity*(const WorldFrame&)>;
    PlayerResolver Player() const;
    std::unique_ptr<AnimationEventPolicy> CreateAnimationPolicy(AnimationTaskPolicy) const;
    void Activate();
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
