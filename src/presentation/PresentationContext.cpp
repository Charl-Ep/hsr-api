#include <hksr/presentation/PresentationContext.hpp>
#include <hksr/model/ModelAccess.hpp>
#include <hksr/presentation/PresentationBindings.hpp>
#include <hksr/presentation/PresentationResources.hpp>
#include <hksr/presentation/PresentationIsolation.hpp>
#include <hksr/presentation/timeline/OwnedTimeline.hpp>
#include <hksr/runtime/RuntimeHost.hpp>
#include <hksr/entity/EntityOwner.hpp>
#include <hksr/entity/NpcCreationSession.hpp>
#include <hksr/entity/EntityCreationSession.hpp>
#include <hksr/entity/MapNpcInputs.hpp>
#include <hksr/animation/ActorAnimationSession.hpp>
#include <hksr/animation/ActorTiming.hpp>
#include "../animation/ActorTimingNative.hpp"
#include "../animation/ActorBgmSyncNative.hpp"
#include <hksr/model/ActorRendering.hpp>
#include "../model/ActorRenderingNative.hpp"
#include <hksr/presentation/effects/EffectLayouts.hpp>
#include "effects/EffectLayoutNative.hpp"
#include "../animation/ActorAnimationParameters.hpp"
#include "../model/ActorModelPresentationNative.hpp"
#include "../model/VisualConfigurationHost.hpp"
#include <hksr/model/ActorModelChange.hpp>
#include "../model/ActorModelChangeNative.hpp"
#include <hksr/model/CharacterOutfit.hpp>
#include <hksr/model/CharacterSkin.hpp>
#include "../model/ActorSkinBindings.hpp"
#include <hksr/model/ModelProfileRequest.hpp>
#include <hksr/model/ModelBaselineCapture.hpp>
#include <hksr/model/ModelVisibilityLease.hpp>
#include <hksr/model/ModelTransition.hpp>
#include <hksr/model/ModelServices.hpp>
#include <hksr/model/ModelServiceBindings.hpp>
#include <hksr/animation/ControllerBinding.hpp>
#include <hksr/presentation/effects/OwnedEffectRequest.hpp>
#include <hksr/presentation/effects/PresentationEffects.hpp>
#include <hksr/presentation/effects/EffectEntityFollow.hpp>
#include <hksr/presentation/effects/EffectSubmissions.hpp>
#include <hksr/presentation/effects/EffectTriggerSource.hpp>
#include <hksr/presentation/events/AnimationExitConfig.hpp>
#include <hksr/presentation/events/AnimationEventPolicy.hpp>
#include "../../adapters/presentation/OwnedAnimationTaskRules.hpp"
#include "audio/ActorAudioHost.hpp"
#include <hksr/presentation/audio/ActorAudio.hpp>
#include <hksr/presentation/audio/OwnedSoundRequest.hpp>
#include <hksr/presentation/movement/OwnedMovementRequest.hpp>
#include <hksr/presentation/movement/MovementGeometry.hpp>
#include "movement/MovementHostFactory.hpp"
#include "attachments/AttachmentVisibilityNative.hpp"
#include "attachments/AttachmentScaleNative.hpp"
#include "materials/FaceMaterialComponents.hpp"
#include <hksr/presentation/projectiles/OwnedProjectileGroup.hpp>
#include <hksr/presentation/events/AnimationEventConstruction.hpp>
#include <hksr/presentation/events/AnimationSequenceExecutions.hpp>
#include <hksr/presentation/events/AnimationEventDefinitions.hpp>
#include <hksr/presentation/events/NpcEventSupplement.hpp>
#include <hksr/presentation/events/NpcEventBindings.hpp>
#include <hksr/control/ActorControlSession.hpp>
#include <hksr/control/PartnerControlSession.hpp>
#include "../../adapters/presentation/ContextBindings.hpp"
#include "../../adapters/control/PartnerNativeBindings.hpp"
#include <hksr/control/ActorControlBindings.hpp>
#include <hksr/control/MouseMode.hpp>
#include <hksr/animation/OfficialCatalog.hpp>
#include <stdexcept>
#include <cmath>
#include <algorithm>

namespace hksr::api {
namespace {
bool Alive(EntityToken);
}
struct PresentationContext::State {
    ManagedObjects& objects;
    PresentationBindings bindings;
    PresentationResources resources;
    std::shared_ptr<const ModelServices> models;
    WorldMap map;
    WorldPlacement placement;
    AnimationControl animation;
    ProfileInputs profile_inputs;
    EntityOwner owner;
    RuntimeMetadata* metadata = nullptr;
    bool prepared = false;
    PresentationIsolationStatus isolation_status;
    PresentationIsolationPolicy isolation_policy;
    Trace trace;
    std::unique_ptr<PresentationIsolation> isolation;
    std::unique_ptr<EffectInputs> effect_inputs;
    std::unique_ptr<EffectControl> effect_control;
    std::unique_ptr<HitEffectControl> hit_effect_control;
    std::unique_ptr<OfficialCatalog> effect_catalog;
    std::unique_ptr<EffectActions> effect_actions;
    std::unique_ptr<EffectAnimatorParameters> effect_parameters;
    std::unique_ptr<EffectTriggerSource> effect_sources;
    bool closing = false;
    std::optional<EffectAccess> effect_access;
    EffectCompletionAllocation completion;
    std::unique_ptr<detail::ActorAudioHost> audio;
    std::shared_ptr<detail::ActorSkinBindings> skins;
    std::optional<MovementRequestServices> movements;
    std::optional<AnimationMovementAccessBinding> movement_access;
    std::unique_ptr<detail::MovementHostFactory> movement_hosts;
    std::shared_ptr<detail::FaceMaterialComponents> face_materials;
    std::unique_ptr<detail::ActorAnimationParameters> animation_parameters;
    std::unique_ptr<detail::ActorTimingFactory> actor_timing;
    std::unique_ptr<detail::ActorBgmSyncFactory> actor_bgm;
    std::unique_ptr<detail::ActorRenderingFactory> actor_rendering;
    std::shared_ptr<EffectLayouts> effect_layouts;
    std::unique_ptr<detail::ActorModelPresentationFactory> model_presentations;
    std::unique_ptr<detail::VisualConfigurationHost> visual_configurations;
    std::unique_ptr<detail::ActorModelChangeFactory> model_changes;
    std::vector<std::unique_ptr<AnimationExitConfig>> exit_inputs;
    ManagedRoots roots;
    std::vector<ManagedRootId> handles;
    std::vector<ManagedRootId> pending_root_releases;
    void* event_type = nullptr;
    void* model_type = nullptr;
    void* style_type = nullptr;
    void* transform_type = nullptr;
    void* character_data_type = nullptr;
    void* visible_type = nullptr;
    std::optional<MovementGeometryAccess> movement_geometry;

    State(ManagedObjects& input, PresentationBindings value)
        : objects(input), bindings(std::move(value)), resources(bindings),
          models(std::make_shared<ModelServices>(
              ModelServiceBindings{bindings.references, bindings.lifetime, bindings.renderer, bindings.entity,
                                   ModelAccess(bindings.model), ArtAdmission(bindings.admission)})),
          map(bindings.world_map), placement(bindings.placement), animation(bindings.animation),
          profile_inputs(bindings.profile_inputs), roots(bindings.references) {
        audio =
            std::make_unique<detail::ActorAudioHost>(objects, bindings.references, models->Entities(), models->Models(),
                                                     owner, resources.Arrays(), resources.Components());
        skins = std::make_shared<detail::ActorSkinBindings>(objects, bindings.references, owner, resources.Arrays(),
                                                            resources.Components());
    }
    void* ComponentType(void*& cached, detail::ActorComponent component) {
        if (!cached) {
            const auto type = detail::ResolveActorComponentType(objects, component);
            if (!type.call.returned || !type.object)
                throw std::runtime_error("actor component Type unavailable");
            handles.push_back(0);
            handles.back() = roots.Keep(type.object);
            const auto kept = roots.Target(handles.back());
            if (!kept.call.returned || !kept.object)
                throw std::runtime_error("actor component Type retention failed");
            cached = kept.object;
        }
        return cached;
    }
    void* EventType() {
        if (!event_type) {
            const auto type = ResolveAnimationInputTypes(objects).events;
            if (!type.call.returned || !type.object)
                throw std::runtime_error("event Type unavailable");
            handles.push_back(0);
            handles.back() = roots.Keep(type.object);
            const auto kept = roots.Target(handles.back());
            if (!kept.call.returned || !kept.object)
                throw std::runtime_error("event Type retention failed");
            event_type = kept.object;
        }
        return event_type;
    }
    void Log(const std::string& text) const noexcept {
        if (trace)
            try {
                trace(text);
            } catch (...) {
            }
    }
    std::shared_ptr<CharacterSkin> Skin(GameEntity* entity, GameWorld* world = nullptr) const {
        return skins->Find(entity, world);
    }
    void PreparationError(const std::string& capability, const std::string& error) {
        const auto message = capability + ": " + error;
        bindings.errors.push_back(message);
        Log(message);
    }
    bool CollectPendingRoots() {
        for (auto it = pending_root_releases.begin(); it != pending_root_releases.end();)
            if (roots.Release(*it).complete) {
                roots.Forget(*it);
                it = pending_root_releases.erase(it);
            } else
                ++it;
        return pending_root_releases.empty();
    }
    ActorAnimatorView MovementModel(EntityToken token, GameWorld* world) {
        if (!world || closing || !Alive(token) || TrackedEntities().IsRetiring(token) ||
            owner.OwnedToken(token.entity).generation != token.generation)
            return {};
        const auto entity = models->Entities().Read(token.entity);
        if (!entity.owner_world.read.returned || entity.owner_world.value != world || !entity.loaded.read.returned ||
            !entity.loaded.value || !Alive(token))
            return {};
        const auto type = ComponentType(model_type, detail::ActorComponent::Model);
        const auto model = models->Entities().Component(token.entity, type);
        if (!model.call.returned || !model.object || !Alive(token))
            return {};
        const auto view = models->Models().Read(static_cast<CharacterModelComponent*>(model.object));
        if (!Alive(token) || !view.changing.read.returned || view.changing.value || !view.animator.read.returned ||
            !view.animator.value)
            return {};
        const auto controller = animation.Controller(view.animator.value);
        return controller.call.returned && controller.object && Alive(token)
                   ? ActorAnimatorView{model.object, view.animator.value, controller.object}
                   : ActorAnimatorView{};
    }
    EffectControl& EffectControls() {
        if (closing)
            throw std::logic_error("presentation effect control closed");
        if (!effect_control) {
            const auto methods = ResolveEffectControl(*metadata);
            for (const auto& error : methods.binding_errors)
                Log("effect control binding: " + error);
            effect_control = std::make_unique<EffectControl>(*effect_access, methods, bindings.references, objects,
                                                             detail::ReadMonoEffectFields(*metadata));
        }
        return *effect_control;
    }
    HitEffectControl& HitEffects() {
        if (closing)
            throw std::logic_error("presentation hit effects closed");
        if (!hit_effect_control)
            hit_effect_control = std::make_unique<HitEffectControl>(
                *effect_access, detail::ResolveHitEffects(*metadata), bindings.references);
        return *hit_effect_control;
    }
    EffectActions& EffectOperations() {
        if (closing)
            throw std::logic_error("presentation effect actions closed");
        if (!effect_actions) {
            const auto methods = ResolveEffectActions(*metadata);
            for (const auto& error : methods.binding_errors)
                Log("effect action binding: " + error);
            effect_actions =
                std::make_unique<EffectActions>(*effect_access, methods, bindings.references, objects,
                                                resources.Arrays(), detail::ReadMonoEffectFields(*metadata));
        }
        return *effect_actions;
    }
    EffectAnimatorParameters& EffectParameters() {
        if (closing)
            throw std::logic_error("presentation effect parameters closed");
        if (!effect_parameters) {
            const auto methods = ResolveEffectAnimatorParameters(*metadata);
            for (const auto& error : methods.binding_errors)
                Log("effect parameter binding: " + error);
            effect_parameters =
                std::make_unique<EffectAnimatorParameters>(*effect_access, methods, bindings.references, objects,
                                                           resources.Arrays(), detail::ReadMonoEffectFields(*metadata));
        }
        return *effect_parameters;
    }
    std::unique_ptr<EventRegistrationLease> ExitEvents(EntityToken entity, const std::string& state,
                                                       const std::vector<EffectRemovalSource>& sources) {
        for (auto it = exit_inputs.begin(); it != exit_inputs.end();) {
            if ((*it)->Close())
                it = exit_inputs.erase(it);
            else
                ++it;
        }
        auto config = std::make_unique<AnimationExitConfig>(*metadata, *metadata, objects, *metadata,
                                                            bindings.references, bindings.arrays);
        auto* input = config.get();
        exit_inputs.push_back(std::move(config));
        const bool ready = input->Prepare(state, sources);
        for (const auto& error : input->Errors())
            Log("action exit input: " + error);
        if (!ready)
            return {};
        if (!event_type) {
            const auto types = ResolveAnimationInputTypes(objects);
            if (!types.events.call.returned || !types.events.object)
                throw std::runtime_error("action exit event Type unavailable");
            handles.push_back(0);
            handles.back() = roots.Keep(types.events.object);
            if (!roots.Status(handles.back()).ready)
                throw std::runtime_error("action exit event Type retention failed");
            event_type = types.events.object;
        }
        const auto component = models->Entities().Component(entity.entity, event_type);
        if (!component.call.returned || !component.object)
            throw std::runtime_error("action exit event component unavailable");
        auto events = std::make_unique<EventRegistrationLease>(bindings.references, AnimationEvents(bindings.events),
                                                               ManagedReferenceList(bindings.event_list));
        events->PrepareInline(input->Groups());
        if (!events->Status().prepared)
            return events;
        events->Attach(entity.entity, component.object, entity);
        input->Close(); // registration now retains the complete private group
        return events;
    }
};
struct PresentationContext::Storage {
    std::shared_ptr<State> state;
};
std::unique_ptr<PresentationContext> PresentationContext::Create(RuntimeMetadata& metadata,
                                                                 RuntimeCapabilities capabilities,
                                                                 PresentationIsolationPolicy policy, Trace trace) {
    auto context = std::make_unique<PresentationContext>(metadata, ResolvePresentationBindings(metadata));
    context->Prepare(metadata, capabilities, policy, std::move(trace));
    return context;
}
PresentationContext::PresentationContext(ManagedObjects& objects, PresentationBindings bindings)
    : storage_(new Storage{std::make_shared<State>(objects, std::move(bindings))}) {}
PresentationContext::~PresentationContext() {
    delete storage_;
}
const PresentationBindings& PresentationContext::Bindings() const {
    return storage_->state->bindings;
}
const PresentationResources& PresentationContext::Resources() const {
    return storage_->state->resources;
}
std::shared_ptr<const ModelServices> PresentationContext::Models() const {
    return storage_->state->models;
}
std::unique_ptr<ManagedRoots> PresentationContext::CreateRoots() const {
    return std::make_unique<ManagedRoots>(storage_->state->bindings.references);
}
CharacterSkinCatalog PresentationContext::ReadSkins() const {
    return storage_->state->skins->Read();
}
std::shared_ptr<CharacterSkin> PresentationContext::PrepareSkin(std::uint32_t id) const {
    return storage_->state->skins->PrepareSkin(id);
}
WorldMapSnapshot PresentationContext::ReadMap(const WorldFrame& frame) const {
    return storage_->state->map.Read(frame);
}
PlacementSnapshot PresentationContext::ReadPlayerPlacement(const WorldFrame& frame) const {
    return storage_->state->placement.ReadPlayer(frame);
}
const std::vector<std::string>& PresentationContext::BindingErrors() const {
    return storage_->state->bindings.errors;
}
std::unique_ptr<AttachmentVisibility> PresentationContext::CreateAttachmentVisibility(EntityToken token,
                                                                                      GameWorld* world) const {
    const auto s = storage_->state;
    if (!s->metadata || !world || !Alive(token) || s->owner.OwnedToken(token.entity).generation != token.generation)
        throw std::runtime_error("attachment visibility requires an owned live actor");
    auto* model_type = s->ComponentType(s->model_type, detail::ActorComponent::Model);
    const auto model = s->models->Entities().Component(token.entity, model_type);
    if (!model.call.returned || !model.object)
        throw std::runtime_error("attachment model unavailable");
    const auto art = s->models->Models().Art(static_cast<CharacterModelComponent*>(model.object));
    if (!art.read.returned || !art.value)
        throw std::runtime_error("attachment Art unavailable");
    auto current = [s, token, world, model = model.object, art = art.value] {
        if (!Alive(token) || s->owner.OwnedToken(token.entity).generation != token.generation)
            return false;
        const auto entity = s->models->Entities().Read(token.entity);
        if (!entity.owner_world.read.returned || entity.owner_world.value != world)
            return false;
        const auto actual = s->models->Entities().Component(token.entity, s->model_type);
        if (!actual.call.returned || actual.object != model)
            return false;
        const auto now = s->models->Models().Art(static_cast<CharacterModelComponent*>(model));
        return Alive(token) && now.read.returned && now.value == art;
    };
    if (!current())
        throw std::runtime_error("attachment model replaced during capture");
    auto* type = s->ComponentType(s->visible_type, detail::ActorComponent::Visibility);
    const auto component = s->models->Entities().Component(token.entity, type);
    if (!component.call.returned || !component.object || !current())
        throw std::runtime_error("attachment visibility component unavailable");
    auto visible_current = [s, token, current = std::move(current), component = component.object] {
        if (!current())
            return false;
        const auto value = s->models->Entities().Component(token.entity, s->visible_type);
        return value.call.returned && value.object == component;
    };
    return detail::NativeAttachmentVisibility(*s->metadata, *s->metadata, s->objects, s->bindings.references,
                                              s->resources.Arrays(), component.object, std::move(visible_current));
}
std::unique_ptr<AttachmentScale> PresentationContext::CreateAttachmentScale(EntityToken token, GameWorld* world) const {
    const auto s = storage_->state;
    if (!s->metadata || !world || !Alive(token) || s->owner.OwnedToken(token.entity).generation != token.generation)
        throw std::runtime_error("attachment scale requires an owned live actor");
    auto* type = s->ComponentType(s->model_type, detail::ActorComponent::Model);
    const auto model = s->models->Entities().Component(token.entity, type);
    if (!model.call.returned || !model.object)
        throw std::runtime_error("attachment scale model unavailable");
    const auto art = s->models->Models().Art(static_cast<CharacterModelComponent*>(model.object));
    if (!art.read.returned || !art.value)
        throw std::runtime_error("attachment scale Art unavailable");
    auto current = [s, token, world, model = model.object, art = art.value] {
        if (!Alive(token) || s->owner.OwnedToken(token.entity).generation != token.generation)
            return false;
        const auto entity = s->models->Entities().Read(token.entity);
        if (!entity.owner_world.read.returned || entity.owner_world.value != world)
            return false;
        const auto actual = s->models->Entities().Component(token.entity, s->model_type);
        if (!actual.call.returned || actual.object != model)
            return false;
        const auto now = s->models->Models().Art(static_cast<CharacterModelComponent*>(model));
        return Alive(token) && now.read.returned && now.value == art;
    };
    if (!current())
        throw std::runtime_error("attachment scale model replaced during capture");
    return detail::NativeAttachmentScale(*s->metadata, *s->metadata, s->objects, s->bindings.references, model.object,
                                         std::move(current));
}
std::unique_ptr<OfficialCatalog> PresentationContext::CreateCatalog() const {
    const auto& s = *storage_->state;
    if (!s.metadata)
        throw std::logic_error("official catalog requires a prepared runtime context");
    return std::make_unique<OfficialCatalog>(*s.metadata, *s.metadata, s.objects, s.bindings.references,
                                             s.resources.Arrays(), s.resources.Components());
}
std::unique_ptr<ActorModelPresentation> PresentationContext::CreateActorModelPresentation(EntityToken token,
                                                                                          GameWorld* world) const {
    auto& s = *storage_->state;
    if (!s.model_presentations)
        throw ModelPresentationUnsupported("model presentation bindings unavailable");
    return s.model_presentations->Create(token, world);
}
std::unique_ptr<ActorVisualConfiguration> PresentationContext::CreateActorVisualConfiguration(EntityToken token,
                                                                                              GameWorld* world) const {
    auto& s = *storage_->state;
    if (!s.visual_configurations || s.closing)
        throw std::runtime_error("visual configuration bindings unavailable");
    return s.visual_configurations->Create(token, world);
}
std::unique_ptr<ActorModelChange> PresentationContext::CreateActorModelChange(EntityToken token, GameWorld* world,
                                                                              std::optional<std::string> path) const {
    auto& s = *storage_->state;
    if (!s.model_changes || s.closing)
        throw std::runtime_error("skill model change bindings unavailable");
    return s.model_changes->Create(token, world, std::move(path));
}
std::unique_ptr<ModelScaleProgram> PresentationContext::CreateModelScaleProgram() const {
    auto& s = *storage_->state;
    if (!s.model_presentations)
        throw ModelPresentationUnsupported("model presentation bindings unavailable");
    return s.model_presentations->CreateScaleProgram();
}
std::unique_ptr<MouseModeLease> PresentationContext::CreateMouseMode() const {
    const auto& s = *storage_->state;
    if (!s.metadata)
        throw std::logic_error("mouse mode requires a prepared runtime context");
    const auto methods = ResolveMouseMode(*s.metadata);
    ObserveMouseModeOwners(methods);
    return std::make_unique<MouseModeLease>(methods, s.bindings.references);
}
namespace {
bool Alive(EntityToken token) {
    return token.entity && !TrackedEntities().IsDisposed(token) &&
           TrackedEntities().Current(token.entity).generation == token.generation;
}
}
PlacementSnapshot PresentationContext::ReadEntityPlacement(EntityToken token, GameWorld* world) const {
    const auto s = storage_->state;
    PlacementSnapshot missing;
    missing.error = "entity placement lifetime/world unavailable";
    if (!world || !Alive(token))
        return missing;
    const auto state = s->models->Entities().Read(token.entity);
    if (!Alive(token) || !state.owner_world.read.returned || state.owner_world.value != world)
        return missing;
    const auto result = s->placement.ReadEntity(token.entity);
    return Alive(token) ? result : missing;
}
Observed<Position3> PresentationContext::ReadLogicalPosition(EntityToken token, GameWorld* world) const {
    auto& s = *storage_->state;
    if (!world || !Alive(token) || s.owner.OwnedToken(token.entity).generation != token.generation)
        return {};
    const auto entity = s.models->Entities().Read(token.entity);
    if (!Alive(token) || !entity.owner_world.read.returned || entity.owner_world.value != world ||
        !entity.loaded.read.returned || !entity.loaded.value)
        return {};
    const auto type = s.ComponentType(s.transform_type, detail::ActorComponent::Transform);
    if (!Alive(token))
        return {};
    const auto transform = s.models->Entities().Component(token.entity, type);
    if (!Alive(token) || !transform.call.returned || !transform.object)
        return {};
    const auto position = HitGeometry(s.bindings.hit_geometry).LogicalPosition(transform.object);
    return Alive(token) ? position : Observed<Position3>{};
}
std::optional<ActorAnimatorView> PresentationContext::ReadAnimator(EntityToken token) const {
    auto& s = *storage_->state;
    if (!Alive(token))
        return {};
    const auto type = s.ComponentType(s.model_type, detail::ActorComponent::Model);
    if (!Alive(token))
        return {};
    const auto component = s.models->Entities().Component(token.entity, type);
    if (!component.call.returned || !component.object || !Alive(token))
        return {};
    const auto model = s.models->Models().Read(static_cast<CharacterModelComponent*>(component.object));
    if (!Alive(token) || !model.changing.read.returned || model.changing.value || !model.animator.read.returned ||
        !model.animator.value)
        return {};
    const auto initialized = s.profile_inputs.AnimatorInitialized(model.animator.value);
    if (!initialized.read.returned || !initialized.value || !Alive(token))
        return {};
    const auto controller = s.animation.Controller(model.animator.value);
    if (!controller.call.returned || !controller.object || !Alive(token))
        return {};
    return ActorAnimatorView{component.object, model.animator.value, controller.object};
}
void* PresentationContext::ReadEventReceiver(EntityToken token, GameWorld* world) const {
    auto& s = *storage_->state;
    if (!world || !Alive(token))
        return nullptr;
    const auto entity = s.models->Entities().Read(token.entity);
    if (!Alive(token) || !entity.owner_world.read.returned || entity.owner_world.value != world ||
        !entity.loaded.read.returned || !entity.loaded.value)
        return nullptr;
    s.EventType();
    if (!Alive(token))
        return nullptr;
    const auto component = s.models->Entities().Component(token.entity, s.event_type);
    if (!Alive(token) || !component.call.returned || !component.object)
        return nullptr;
    const auto receiver = AnimationEvents(s.bindings.events).Consumer(component.object);
    return Alive(token) && receiver.call.returned ? receiver.object : nullptr;
}
void* PresentationContext::NpcMotionComponent(EntityToken token, GameWorld* world) const {
    auto& s = *storage_->state;
    if (!Alive(token))
        throw std::runtime_error("selected character is retiring");
    const auto& entities = s.models->Entities();
    const auto state = entities.Read(token.entity);
    if (!Alive(token))
        throw std::runtime_error("selected character ended during query");
    if (!state.owner_world.read.returned || state.owner_world.value != world || !state.loaded.read.returned ||
        !state.loaded.value)
        throw std::runtime_error("selected character is not loaded in the current world");
    const auto type = s.ComponentType(s.style_type, detail::ActorComponent::FreeStyle);
    if (!Alive(token))
        throw std::runtime_error("selected character ended during query");
    const auto component = entities.Component(token.entity, type);
    if (!component.call.returned || !component.object || !Alive(token))
        throw std::runtime_error("own FreeStyle component unavailable");
    return component.object;
}
std::optional<MovementGeometry> PresentationContext::ReadMovementGeometry(EntityToken token, GameWorld* world) const {
    auto& s = *storage_->state;
    if (!s.metadata || s.closing)
        throw std::logic_error("movement geometry context unavailable");
    if (!Alive(token))
        throw std::runtime_error("movement geometry entity ended");
    const auto state = s.models->Entities().Read(token.entity);
    if (!state.owner_world.read.returned || state.owner_world.value != world || !Alive(token))
        throw std::runtime_error("movement geometry world changed");
    const auto type = s.ComponentType(s.character_data_type, detail::ActorComponent::CharacterData);
    const auto component = s.models->Entities().Component(token.entity, type);
    if (!component.call.returned || !Alive(token))
        throw std::runtime_error("movement geometry component unread");
    if (!component.object)
        return {};
    if (!s.movement_geometry)
        s.movement_geometry = detail::ResolveActorMovementGeometry(*s.metadata);
    const auto value = s.movement_geometry->Read(component.object);
    if (!value.read.returned || !Alive(token))
        throw std::runtime_error("movement geometry values unread");
    return value.value;
}
const EntityOwner& PresentationContext::Owner() const {
    return storage_->state->owner;
}
std::unique_ptr<ActorTiming> PresentationContext::CreateActorTiming(EntityToken token, GameWorld* world) const {
    const auto& s = *storage_->state;
    if (!s.actor_timing || s.closing)
        throw std::runtime_error("actor timing bindings unavailable");
    return s.actor_timing->Create(token, world);
}
std::unique_ptr<ActorBgmSync> PresentationContext::CreateActorBgmSync(EntityToken token, GameWorld* world) const {
    auto& s = *storage_->state;
    if (!s.actor_bgm || s.closing)
        throw std::runtime_error("actor BGM sync bindings unavailable");
    return s.actor_bgm->Create(token, world);
}
std::unique_ptr<ActorRendering> PresentationContext::CreateActorRendering(EntityToken token, GameWorld* world) const {
    auto& s = *storage_->state;
    if (!s.actor_rendering || s.closing)
        throw std::runtime_error("actor rendering bindings unavailable");
    return s.actor_rendering->Create(token, world);
}
std::shared_ptr<EffectLayouts> PresentationContext::CreateEffectLayouts() const {
    auto& s = *storage_->state;
    if (s.closing || !s.effect_layouts)
        throw std::runtime_error("effect layout bindings unavailable");
    return s.effect_layouts;
}
PresentationContext::NpcFactory PresentationContext::Npcs() const {
    return [s = storage_->state, observer = BirthObserver()] {
        const auto& b = s->bindings;
        return std::make_unique<NpcCreationSession>(s->owner, b.factory, b.entity, b.references, observer);
    };
}
PresentationContext::AnimationFactory PresentationContext::Animations() const {
    return [s = storage_->state](EntityToken owner) {
        const auto &b = s->bindings;
        auto session = std::make_unique<ActorAnimationSession>(
            owner, b.references, EntityAccess(b.entity), ModelAccess(b.model), AnimationControl(b.animation),
            ProfileInputs(b.profile_inputs),
            ActorHitGeometryBindings{b.hit_geometry, b.hit_direction, b.placement, b.scene});
        session->BindParameters([s, owner](GameWorld *world, const AnimationParameter &parameter,
                                           AnimationParameterPropagation propagation) {
            AnimationParameterResult out;
            if (!s->animation_parameters || s->closing) {
                out.detail = "animation parameter bindings unavailable";
                return out;
            }
            try {
                return s->animation_parameters->Apply(
                    owner, world, parameter, propagation,
                    [s] { return s->effect_inputs ? s->effect_inputs->Manager() : ObjectResult{}; },
                    [s](EffectReference ref, const EffectRelationsSnapshot& facts) {
                        return s->isolation ? s->isolation->Source(ref, facts) : EntityOrigin{};
                    });
            } catch (const std::exception& e) {
                out.state = AnimationParameterState::Failed;
                out.detail = e.what();
                return out;
            }
        });
        return session;
    };
}
PresentationContext::EntityFactory PresentationContext::Entities() const {
    return Entities({});
}
PresentationContext::EntityFactory PresentationContext::Entities(std::shared_ptr<CharacterSkin> skin) const {
    auto s = storage_->state;
    Method story;
    if (s->metadata) {
        story = ResolveStoryActorFactory(*s->metadata);
        if (!s->face_materials)
            s->face_materials = std::make_shared<detail::FaceMaterialComponents>(
                *s->metadata, s->bindings.references, s->bindings.entity, s->resources.Arrays(), s->owner);
    }
    return [s, story, observer = SkinBirthObserver(std::move(skin))] {
        auto session = std::make_unique<EntityCreationSession>(s->owner, s->bindings.factory, story, &s->objects,
                                                               s->bindings.entity, s->bindings.references, observer);
        if (s->metadata)
            session->PrepareStoryRuntime(
                *s->metadata,
                [factory = s->face_materials](EntityToken owner, GameWorld* world) { factory->Ensure(owner, world); });
        return session;
    };
}
std::function<void(EntityOrigin)> PresentationContext::SkinBirthObserver(std::shared_ptr<CharacterSkin> skin) const {
    return storage_->state->skins->BirthObserver(std::move(skin), BirthObserver());
}
PresentationContext::EntityEventFactory PresentationContext::EntityEvents() const {
    auto s = storage_->state;
    std::size_t story_offset = 0;
    if (s->metadata)
        story_offset = detail::ResolveStoryEventOffset(*s->metadata);
    return [s, story_offset](EntityCreationSession& entity, GameWorld* world) {
        const auto& b = s->bindings;
        const auto& r = s->resources;
        return std::make_unique<NpcEventSupplement>(entity, world, b.references,
                                                    NpcEventBindings{EntityAccess(b.entity), b.adventure_data,
                                                                     r.Config(), AnimationEvents(b.events), r.Arrays(),
                                                                     ManagedReferenceList(b.event_list), r.Scene()},
                                                    story_offset);
    };
}
PresentationContext::EntityFactory PresentationContext::Partners() const {
    return Partners({});
}
PresentationContext::EntityFactory PresentationContext::Partners(std::shared_ptr<CharacterSkin> skin) const {
    auto s = storage_->state;
    Method partner;
    if (s->metadata)
        partner = ResolvePartnerFactory(*s->metadata);
    return [s, partner, observer = SkinBirthObserver(std::move(skin))] {
        auto session = std::make_unique<EntityCreationSession>(
            s->owner, Method{}, Method{}, &s->objects, s->bindings.entity, s->bindings.references, observer, partner);
        if (s->metadata)
            session->PreparePartnerRuntime(*s->metadata);
        return session;
    };
}
std::unique_ptr<PartnerControlSession> PresentationContext::PartnerControls(EntityToken partner, EntityToken summoner,
                                                                            GameWorld* world) const {
    auto& s = *storage_->state;
    if (!s.metadata)
        throw std::logic_error("partner controls require a prepared runtime context");
    auto methods = detail::ResolvePartnerControlBindings(*s.metadata);
    return std::make_unique<PartnerControlSession>(s.owner, partner, summoner, world, methods, s.bindings.references,
                                                   s.objects);
}
PresentationContext::NpcInputFactory PresentationContext::NpcInputs() const {
    if (!storage_->state->metadata)
        return {};
    return [s = storage_->state](std::uint32_t id, const char* name, Position3 position, Rotation4 rotation) {
        return MapNpcInputs::Prepare(*s->metadata, s->bindings.references, id, name, position, rotation);
    };
}
PresentationContext::NpcEventFactory PresentationContext::NpcEvents() const {
    return [s = storage_->state](NpcCreationSession& npc, GameWorld* world) {
        const auto& b = s->bindings;
        const auto& r = s->resources;
        return std::make_unique<NpcEventSupplement>(npc, world, b.references,
                                                    NpcEventBindings{EntityAccess(b.entity), b.adventure_data,
                                                                     r.Config(), AnimationEvents(b.events), r.Arrays(),
                                                                     ManagedReferenceList(b.event_list), r.Scene()});
    };
}
void PresentationContext::Prepare(RuntimeMetadata& metadata, RuntimeCapabilities capabilities,
                                  PresentationIsolationPolicy policy, Trace trace) {
    auto& s = *storage_->state;
    if (s.prepared)
        throw std::logic_error("presentation context already prepared");
    s.prepared = true;
    s.trace = std::move(trace);
    s.isolation_policy = policy;
    try {
        InstallMovementPlacement(s.bindings.actor_control);
    } catch (const std::exception& e) {
        s.PreparationError("movement placement unavailable", e.what());
    }
    try {
        if (!capabilities.entity_lifecycle)
            throw std::runtime_error("entity birth observation unavailable");
        s.isolation = std::make_unique<PresentationIsolation>(metadata, s.bindings.references, policy, s.trace);
        s.isolation_status.phase = PresentationIsolationPhase::Prepared;
        s.Log("presentation isolation prepared; activation precedes owned creation; game qualification pending");
    } catch (const std::exception& e) {
        s.isolation_status = {PresentationIsolationPhase::Unavailable, e.what()};
        s.PreparationError("presentation isolation unavailable (original behavior retained)", e.what());
    }
    // Resolve each capability independently from actual declarations. A game
    // version label cannot suppress unrelated creation or model services.
    s.metadata = &metadata;
    s.audio->Prepare(metadata, s.trace);
    s.skins->Prepare(metadata);
    try {
        s.actor_timing = std::make_unique<detail::ActorTimingFactory>(
            metadata, s.bindings.references, s.models->Entities(), s.models->Models(), s.owner);
    } catch (const std::exception& e) {
        s.PreparationError("actor timing unavailable", e.what());
    }
    try {
        s.actor_bgm = std::make_unique<detail::ActorBgmSyncFactory>(
            metadata, s.bindings.references, s.models->Entities(), s.models->Models(), s.resources.Arrays(), s.owner);
    } catch (const std::exception& e) {
        s.PreparationError("actor BGM sync unavailable", e.what());
    }
    try {
        s.actor_rendering = std::make_unique<detail::ActorRenderingFactory>(
            metadata, s.bindings.references, s.models->Entities(), s.models->Models(), s.resources.Arrays(), s.owner);
    } catch (const std::exception& e) {
        s.PreparationError("actor rendering unavailable", e.what());
    }
    try {
        if (!s.face_materials)
            s.face_materials = std::make_shared<detail::FaceMaterialComponents>(
                metadata, s.bindings.references, s.bindings.entity, s.resources.Arrays(), s.owner);
    } catch (const std::exception& error) {
        s.PreparationError("face component preparation unavailable", error.what());
    }
    try {
        s.model_presentations = std::make_unique<detail::ActorModelPresentationFactory>(
            metadata, s.bindings.references, s.models->Entities(), s.models->Models(), s.resources.Arrays(), s.owner,
            [face = s.face_materials](EntityToken owner, GameWorld* world) {
                if (face)
                    face->Ensure(owner, world);
            });
    } catch (const std::exception& error) {
        s.PreparationError("model presentation preparation unavailable", error.what());
    }
    try {
        s.visual_configurations = std::make_unique<detail::VisualConfigurationHost>(
            metadata, metadata, s.objects,
            [&metadata](const char* ns, const char* name) { return metadata.ReadFields(ns, name); },
            s.bindings.references, s.resources.Arrays(), s.models->Entities(),
            [owner = s.owner](EntityToken token) {
                return owner.OwnedToken(token.entity).generation == token.generation;
            },
            AnimationEvents(s.bindings.events));
    } catch (const std::exception& error) {
        s.PreparationError("visual configuration preparation unavailable", error.what());
    }
    try {
        s.model_changes = std::make_unique<detail::ActorModelChangeFactory>(
            metadata, s.bindings.references, s.models->Entities(), s.models->Models(), s.owner);
    } catch (const std::exception& error) {
        s.PreparationError("skill model change preparation unavailable", error.what());
    }
    try {
        s.animation_parameters = std::make_unique<detail::ActorAnimationParameters>(
            metadata, s.bindings.references, s.models->Entities(), s.models->Models(), s.owner);
        if (!s.animation_parameters->PropagationError().empty())
            s.PreparationError("animation parameter propagation unavailable",
                               s.animation_parameters->PropagationError());
    } catch (const std::exception& e) {
        s.PreparationError("animation parameters unavailable", e.what());
    }
    try {
        const auto methods = ResolveEffectAccess(metadata);
        for (const auto& error : methods.binding_errors)
            s.PreparationError("effect binding", error);
        s.completion = ResolveEffectCompletion(metadata);
        s.effect_inputs = std::make_unique<EffectInputs>(metadata, metadata, metadata, s.bindings.references);
        s.effect_access.emplace(methods);
        try {
            s.effect_layouts = detail::PrepareEffectLayouts(metadata, *s.effect_access, s.bindings.references);
        } catch (const std::exception& error) {
            s.PreparationError("effect layout bindings unavailable", error.what());
        }
    } catch (const std::exception& e) {
        s.PreparationError("ready effect services unavailable", e.what());
    }
}
EntityCreationScope::BirthObserver PresentationContext::BirthObserver() const {
    return [s = storage_->state](EntityOrigin origin) {
        if (s->isolation)
            s->isolation->RememberBirth(origin);
    };
}
void PresentationContext::SetCameraPresentations(bool enabled) {
    auto& s = *storage_->state;
    if (!s.isolation)
        throw std::runtime_error("effect visibility service unavailable");
    s.isolation->SetCameraPresentations(enabled);
}
std::function<std::unique_ptr<CharacterOutfit>()> PresentationContext::Outfits() const {
    if (!storage_->state->metadata)
        return {};
    return [s = storage_->state] {
        return std::make_unique<CharacterOutfit>(*s->metadata, *s->metadata, s->objects, s->bindings.references,
                                                 s->bindings.entity);
    };
}
ModelRequestServices PresentationContext::ModelRequests() const {
    const auto& b = storage_->state->bindings;
    std::function<std::unique_ptr<EventValueLease>()> values;
    if (storage_->state->metadata)
        values = [s = storage_->state] {
            return std::make_unique<EventValueInputs>(*s->metadata, *s->metadata, *s->metadata, s->bindings.references);
        };
    const auto pending = storage_->state->metadata ? ResolveModelPending(*storage_->state->metadata) : Method{};
    return {ModelProfile(b.profile, pending),
            ModelAccess(b.model),
            ProfileInputs(b.profile_inputs),
            SceneObjects(b.scene),
            AnimationEvents(b.events),
            ModelLoadCompleted,
            std::move(values)};
}
ControllerBindingServices PresentationContext::Controllers() const {
    const auto& b = storage_->state->bindings;
    return {AnimationControl(b.animation),
            EntityAccess(b.entity),
            SceneObjects(b.scene),
            ManagedArrays(b.arrays),
            b.model_loader,
            b.loader_model,
            b.loader_manager};
}
PresentationContext::BaselineFactory PresentationContext::Baselines() const {
    return [s = storage_->state, requests = ModelRequests()](EntityToken entity, GameWorld* world,
                                                             const ProfileDescription& profile) {
        const auto& b = s->bindings;
        auto capture = std::make_unique<ModelBaselineCapture>(entity, world, b.references, EntityAccess(b.entity),
                                                              requests, b.profile_events, profile);
        if (s->metadata)
            capture->SetVisibilityLease(
                std::make_unique<ModelVisibilityLease>(*s->metadata, s->objects, b.references, s->resources.Arrays()));
        return capture;
    };
}
PresentationContext::ModelTransitionFactory PresentationContext::ModelTransitions() const {
    return [s = storage_->state, controllers = Controllers()](EntityToken entity, GameWorld* world,
                                                              std::unique_ptr<ModelProfileRequest> request) {
        const auto& b = s->bindings;
        return std::make_unique<ModelTransition>(
            entity, world, std::move(request),
            ModelTransitionServices{b.references, EntityAccess(b.entity), ModelAccess(b.model),
                                    AnimationControl(b.animation), controllers, b.profile_events});
    };
}
std::optional<EffectRequestServices> PresentationContext::Effects() const {
    const auto s = storage_->state;
    if (!s->effect_access)
        return {};
    return EffectRequestServices{
        s->bindings.references,
        *s->effect_access,
        s->completion,
        [s] { return s->effect_inputs->Manager(); },
        [s](void* input, void* target, void* callback) { return s->effect_inputs->Configure(input, target, callback); },
        s->trace,
        [s](EffectReference ref, void* entity) {
            if (s->isolation)
                s->isolation->RememberRequest(ref, static_cast<GameEntity*>(entity));
        },
        [s](EffectReference ref) {
            if (s->isolation)
                s->isolation->RetireRequest(ref);
        },
        [s](EntityToken entity, const std::string& state, const std::vector<EffectRemovalSource>& sources) {
            return s->ExitEvents(entity, state, sources);
        },
        [s](void* parameters, void* caster, void* callback, const SpatialEffectOptions& options) {
            return s->effect_inputs->ConfigureSpatial(parameters, caster, callback, options);
        },
        [s](void* parameters, void* caster, void* target, void* callback, const ImpactEffectOptions& options) {
            return s->effect_inputs->ConfigureImpact(parameters, caster, target, callback, options);
        },
        [s](void* parameters, void* caster, void* target, void* callback, const HitEffectPlacement& placement) {
            auto& control = s->HitEffects(); // prepare before any load callback
            auto out = s->effect_inputs->ConfigureHitReferences(parameters, caster, target, callback);
            if (out.target.returned && out.completion.returned)
                out.target = control.Configure(parameters, placement);
            return out;
        },
        [s](EffectReference ref, void* effect, Position3 direction) {
            return s->HitEffects().Loaded(ref, effect, direction);
        },
        [s](void* manager, EffectReference ref, const HitEffectPlacement& placement) {
            return s->HitEffects().Update(manager, ref, placement);
        },
        [s](EffectReference ref, EffectMutexKind kind) { return s->HitEffects().Replay(ref, kind); },
        [s](void* caster, void* target, void* path, const EffectMutexRule& rule, float delay,
            const std::vector<EffectReference>& candidates) {
            const auto manager = s->effect_inputs->Manager();
            if (!manager.call.returned || !manager.object)
                throw std::runtime_error("effect mutex manager unavailable");
            return s->HitEffects().Mutex(manager.object, caster, target, path, rule, delay, candidates);
        },
        [s](const std::string& path) {
            if (!s->effect_catalog)
                s->effect_catalog =
                    std::make_unique<OfficialCatalog>(*s->metadata, *s->metadata, s->objects, s->bindings.references,
                                                      s->resources.Arrays(), s->resources.Components());
            return s->effect_catalog->ReadHitEffectRules(path);
        },
        [s](EffectReference ref, void* caster, void* path) { return s->HitEffects().Matches(ref, caster, path); },
        [s](void* parameters, bool enabled) { return s->effect_inputs->ConfigureReplay(parameters, enabled); }};
}
std::shared_ptr<const PresentationEffects> PresentationContext::EffectServices() const {
    auto services = Effects();
    if (!services)
        return {};
    const auto s = storage_->state;
    EffectControlServices control;
    control.progress = [s](EffectReference ref, float value, float duration) {
        return s->EffectControls().SetProgress(ref, value, duration);
    };
    control.animator = [s](EffectReference ref, int hash, float duration, bool restart) {
        return s->EffectControls().SetAnimatorState(ref, hash, duration, restart);
    };
    control.named_animator = [s](EffectReference ref, const std::string& name, float duration, bool restart) {
        return s->EffectControls().SetAnimatorState(ref, name, duration, restart);
    };
    EffectActionServices actions;
    actions.trigger = [s](EffectReference ref, const std::string& name, bool reset) {
        return s->EffectOperations().Trigger(ref, name, reset);
    };
    actions.remove = [s](void* manager, EffectReference ref, const EffectRemovalOptions& options) {
        return s->EffectOperations().Remove(manager, ref, options);
    };
    actions.parameter = [s](EffectReference ref, const EffectAnimatorParameter& value) {
        return s->EffectParameters().Set(ref, value);
    };
    return std::make_shared<PresentationEffects>(
        std::move(*services), std::move(control),
        [s](void* parameters, void* caster, void* target, void* callback, const EntityEffectOptions& options) {
            return s->effect_inputs->ConfigureEntity(parameters, caster, target, callback, options);
        },
        std::move(actions));
}
PresentationIsolationStatus PresentationContext::IsolationStatus() const {
    return storage_->state->isolation_status;
}
std::unique_ptr<EffectEntityFollow> PresentationContext::CreateEffectEntityFollow() const {
    const auto s = storage_->state;
    if (!s->metadata || s->closing)
        throw std::logic_error("presentation effects are not prepared or are closed");
    auto& metadata = *s->metadata;
    auto follow = detail::ResolveEffectFollowBindings(metadata);
    if (!s->effect_access) {
        follow.methods = {};
        follow.methods.binding_errors.push_back("effect request access unavailable");
    }
    for (const auto& error : follow.methods.binding_errors)
        s->Log("effect follow binding: " + error);
    for (const auto& error : follow.list.binding_errors)
        s->Log("effect follow list: " + error);
    return std::make_unique<EffectEntityFollow>(s->effect_access.value_or(EffectAccess(EffectAccessMethods{})),
                                                std::move(follow.methods), follow.fields, s->bindings.references,
                                                s->objects, s->resources.Arrays(),
                                                ManagedReferenceList(std::move(follow.list)), follow.motion);
}
std::unique_ptr<OwnedTimeline> PresentationContext::CreateTimeline(EntityToken token, GameWorld* world) const {
    const auto s = storage_->state;
    if (!s->isolation)
        throw std::logic_error("Timeline playback requires presentation isolation");
    return s->isolation->CreateTimeline({token, world});
}
std::unique_ptr<OwnedSoundRequest> PresentationContext::StartSound(EntityToken token, GameWorld* world,
                                                                   const std::string& event) const {
    return storage_->state->audio->Start(token, world, event);
}
void PresentationContext::SetActorAudio(EntityToken token, GameWorld* world, std::vector<ActorAudioEvent> events,
                                        unsigned rate) {
    storage_->state->audio->Set(token, world, std::move(events), rate);
}
void PresentationContext::SetActorAudioRate(EntityToken token, GameWorld* world, unsigned rate) {
    storage_->state->audio->SetRate(token, world, rate);
}
void PresentationContext::ClearActorAudio(EntityToken token, GameWorld* world) {
    storage_->state->audio->Clear(token, world);
}
std::string PresentationContext::ReadVoiceEvent(EntityToken token, GameWorld* world, std::uint32_t avatar,
                                                VoiceCue cue) const {
    auto& s = *storage_->state;
    auto event = s.audio->ReadVoiceEvent(token, world, avatar, cue);
    if (event.empty())
        return event;
    if (const auto skin = s.skins->Find(token.entity, world))
        event = skin->SelectSound(event);
    return s.audio->VoiceExists(event) ? event : std::string{};
}
std::optional<VoiceCue> PresentationContext::ReadReadyVoiceCue(std::int32_t type) const {
    return storage_->state->audio->ReadReadyVoiceCue(type);
}
std::vector<std::string> PresentationContext::ReadActorEventFiles(EntityToken token, GameWorld* world) const {
    auto& s = *storage_->state;
    if (!world || !Alive(token) || s.owner.OwnedToken(token.entity).generation != token.generation)
        throw std::invalid_argument("event files require this client's owned actor");
    auto* receiver = ReadEventReceiver(token, world);
    if (!receiver)
        throw std::runtime_error("actor event consumer unavailable");
    struct Pins {
        ManagedRoots& roots;
        std::vector<ManagedRootId>& pending;
        std::vector<ManagedRootId> ids;
        void Keep(void* object) {
            if (!object)
                return;
            ids.push_back(0);
            ids.back() = roots.Keep(object);
            if (!roots.Status(ids.back()).ready)
                throw std::runtime_error("actor event file retention failed");
        }
        ~Pins() {
            for (auto id : ids)
                if (id) {
                    if (roots.Release(id).complete)
                        roots.Forget(id);
                    else
                        pending.push_back(id);
                }
        }
    } pins{s.roots, s.pending_root_releases, {}};
    s.pending_root_releases.reserve(s.pending_root_releases.size() + 2);
    pins.ids.reserve(2);
    pins.Keep(receiver);
    const auto input = ReadProfileEventArrays(receiver, s.bindings.profile_events);
    if (!input.paths.call.returned)
        throw std::runtime_error("adopted actor event files unreadable");
    if (!input.paths.object)
        return {};
    pins.Keep(input.paths.object);
    const auto count = s.resources.Arrays().Length(input.paths.object);
    if (!count.read.returned || count.value < 0)
        throw std::runtime_error("actor event file count unavailable");
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(count.value));
    for (int i = 0; i < count.value; ++i) {
        const auto item = s.resources.Arrays().Get(input.paths.object, i);
        if (!item.call.returned)
            throw std::runtime_error("actor event file item unavailable");
        if (!item.object)
            continue;
        const auto text = s.resources.Components().ReadText(item.object);
        if (!text.read.returned)
            throw std::runtime_error("actor event file text unavailable");
        result.push_back(text.utf8);
    }
    return result;
}
std::shared_ptr<OwnedMovementHost> PresentationContext::AcquireMovementHost(EntityToken token, GameWorld* world) const {
    const auto s = storage_->state;
    if (!s->metadata || s->closing)
        throw std::logic_error("movement context unavailable");
    const auto model = s->MovementModel(token, world);
    if (!model.animator)
        throw std::runtime_error("movement requires this client's current owned model");
    if (!s->movement_hosts)
        s->movement_hosts = std::make_unique<detail::MovementHostFactory>(*s->metadata, s->bindings.references,
                                                                          s->resources.Arrays(), s->trace);
    const auto current = [s, token, world, model] {
        const auto now = s->MovementModel(token, world);
        return now.component == model.component && now.animator == model.animator && now.controller == model.controller;
    };
    return s->movement_hosts->Acquire(token, model.animator, current, [token] { return Alive(token); });
}
ProjectileLaunch PresentationContext::CaptureProjectileTarget(EntityToken token, GameWorld* world) const {
    const auto s = storage_->state;
    const auto read = [s, token, world]() -> std::pair<void*, void*> {
        const auto live = [&] {
            return world && !s->closing && Alive(token) && !TrackedEntities().IsRetiring(token) &&
                   s->owner.OwnedToken(token.entity).generation == token.generation;
        };
        if (!live())
            return {};
        const auto entity = s->models->Entities().Read(token.entity);
        if (!live() || !entity.owner_world.read.returned || entity.owner_world.value != world ||
            !entity.loaded.read.returned || !entity.loaded.value)
            return {};
        const auto type = s->ComponentType(s->model_type, detail::ActorComponent::Model);
        if (!live())
            return {};
        const auto model = s->models->Entities().Component(token.entity, type);
        if (!live() || !model.call.returned || !model.object)
            return {};
        const auto art = s->models->Models().Art(static_cast<CharacterModelComponent*>(model.object));
        return live() && art.read.returned && art.value ? std::make_pair(model.object, art.value)
                                                        : std::pair<void*, void*>{};
    };
    const auto model = read();
    if (!model.first)
        throw std::runtime_error("projectile target requires this client's current owned model");
    ProjectileLaunch out;
    out.target = {token, world};
    out.retained_inputs = {model.first, model.second};
    out.target_current = [read, model] { return read() == model; };
    return out;
}
std::unique_ptr<OwnedProjectileGroup>
PresentationContext::CreateProjectiles(EntityToken token, GameWorld* world,
                                       std::function<void(const ProjectileArrival&)> arrival) const {
    const auto s = storage_->state;
    if (!s->metadata || s->closing)
        throw std::logic_error("projectile context unavailable");
    auto owner = CaptureProjectileTarget(token, world);
    ProjectileGroupServices services;
    services.methods = s->metadata;
    services.metadata = s->metadata;
    services.objects = &s->objects;
    services.fields = [s](const char* ns, const char* type) { return s->metadata->ReadFields(ns, type); };
    services.references = s->bindings.references;
    services.arrays = s->bindings.arrays;
    services.lifecycle = &TrackedEntities();
    services.effects = EffectServices();
    services.owner_current = std::move(owner.target_current);
    services.retained_inputs = std::move(owner.retained_inputs);
    services.arrival = std::move(arrival);
    services.trace = s->trace;
    auto group = std::make_unique<OwnedProjectileGroup>(std::move(services), EntityOrigin{token, world});
    if (auto skin = s->Skin(token.entity, world))
        group->SetResourceTransform([skin](const auto& path) { return skin->MapResource(path); });
    return group;
}
std::unique_ptr<OwnedMovementRequest> PresentationContext::StartMovement(EntityToken token, GameWorld* world,
                                                                         const std::string& state,
                                                                         AnimationMovePlan plan) const {
    const auto s = storage_->state;
    if (!s->metadata || s->closing)
        throw std::logic_error("movement context unavailable");
    if (state.empty() || state.find('\0') != std::string::npos)
        throw std::invalid_argument("movement animation state is empty or contains NUL");
    const auto source = [s, token, world] { return s->MovementModel(token, world); };
    if (!s->movements) {
        MovementRequestServices value;
        value.data = ResolveAnimationMoveData(*s->metadata);
        if (!s->movement_access)
            s->movement_access = ResolveAnimationMovementAccess(*s->metadata);
        value.access = *s->movement_access;
        value.update = ResolveAnimationMovementUpdate(*s->metadata);
        value.lifecycle = &TrackedEntities();
        s->movements = std::move(value);
    }
    const auto model = source();
    if (!model.animator)
        throw std::runtime_error("movement requires this client's current owned model");
    std::vector<ManagedRootId> temporary;
    const auto keep = [&](void* object) {
        temporary.reserve(temporary.size() + 1);
        s->handles.reserve(s->handles.size() + 1);
        const auto id = s->roots.Keep(object);
        temporary.push_back(id);
        s->handles.push_back(id);
        if (!s->roots.Status(id).ready)
            throw std::runtime_error("movement preparation retention failed");
    };
    const auto release = [&] {
        for (auto id : temporary)
            if (s->roots.Release(id).complete) {
                s->roots.Forget(id);
                for (auto it = s->handles.begin(); it != s->handles.end(); ++it)
                    if (*it == id) {
                        s->handles.erase(it);
                        break;
                    }
            }
        temporary.clear(); // failed frees remain owned by the context
    };
    try {
        keep(token.entity);
        keep(model.component);
        keep(model.animator);
        keep(model.controller);
        const auto current = [source, model] {
            const auto now = source();
            return now.component == model.component && now.animator == model.animator &&
                   now.controller == model.controller;
        };
        if (!current())
            throw std::runtime_error("movement model changed during preparation");
        const auto name = s->objects.NewStringUtf8(state.c_str());
        if (!name.call.returned || !name.object)
            throw std::runtime_error("movement state name allocation failed");
        keep(name.object);
        if (!current())
            throw std::runtime_error("movement model changed during preparation");
        const auto key = s->animation.StateHash(name.object);
        const auto member = key.read.returned ? s->animation.HasState(model.animator, 0, key.value) : Observed<bool>{};
        if (!key.read.returned || !member.read.returned || !member.value)
            throw std::runtime_error("movement state is absent from the owned Animator base layer");
        if (!current())
            throw std::runtime_error("movement model changed during preparation");
        const auto origin = s->placement.ReadEntity(token.entity);
        if (!origin.ready)
            throw std::runtime_error("movement origin unavailable: " + origin.error);
        plan.origin = origin.position;
        plan.animation_key = key.value;
        auto services = *s->movements;
        services.model_objects = {model.component, model.animator, model.controller};
        services.current = current;
        services.alive = [token] { return Alive(token); };
        services.host = AcquireMovementHost(token, world);
        if (!services.host->Status().prepared)
            throw std::runtime_error(services.host->Status().error);
        services.origin = [s, token] {
            const auto pose = s->placement.ReadEntity(token.entity);
            if (!pose.ready)
                throw std::runtime_error("movement origin unavailable: " + pose.error);
            return pose.position;
        };
        auto* component = services.host->Component();
        auto request = std::make_unique<OwnedMovementRequest>(std::move(services), token, component);
        request->Start(plan);
        release();
        return request;
    } catch (...) {
        release();
        throw;
    }
}
std::unique_ptr<OwnedMovementRequest> PresentationContext::StartPosition(EntityToken token, GameWorld* world,
                                                                         Position3 position) const {
    const auto s = storage_->state;
    if (!s->metadata || s->closing)
        throw std::logic_error("movement context unavailable");
    const auto model = s->MovementModel(token, world);
    if (!model.animator)
        throw std::runtime_error("positioning requires this client's current owned model");
    MovementRequestServices services;
    services.data.references = s->bindings.references;
    if (!s->movement_access)
        s->movement_access = ResolveAnimationMovementAccess(*s->metadata);
    services.access = *s->movement_access;
    services.lifecycle = &TrackedEntities();
    services.model_objects = {model.component, model.animator, model.controller};
    services.alive = [token] { return Alive(token); };
    services.current = [s, token, world, model] {
        const auto now = s->MovementModel(token, world);
        return now.component == model.component && now.animator == model.animator && now.controller == model.controller;
    };
    auto host = AcquireMovementHost(token, world);
    if (!host->Status().prepared)
        throw std::runtime_error(host->Status().error);
    services.host = host;
    try {
        auto request = std::make_unique<OwnedMovementRequest>(std::move(services), token, host->Component());
        request->StartPosition(position);
        return request;
    } catch (...) {
        host->Close();
        throw;
    }
}
TextResult PresentationContext::ReadEffectTriggerPath(const EffectTriggerExecution& trigger) const {
    auto& s = *storage_->state;
    if (!s.metadata)
        throw std::logic_error("effect source reading requires prepared metadata");
    if (!s.effect_sources)
        s.effect_sources = std::make_unique<EffectTriggerSource>(detail::ReadTriggerEffectFields(*s.metadata),
                                                                 s.bindings.references, s.resources.Components());
    return s.effect_sources->Path(trigger);
}
std::unique_ptr<EffectSubmissions>
PresentationContext::CreateEffectObserver(std::function<void(const EffectSubmission&)> before,
                                          std::function<void(const EffectSubmission&, bool)> after) const {
    const auto s = storage_->state;
    if (!s->metadata || !s->isolation)
        throw std::logic_error("effect observation requires a prepared isolation context");
    return std::make_unique<EffectSubmissions>(
        *s->metadata, s->bindings.references,
        [s](const char* ns, const char* type) { return s->metadata->ReadFields(ns, type); },
        [s](GameEntity* entity) { return s->isolation->Source(entity); }, std::move(before), std::move(after),
        s->trace);
}
PresentationContext::ControlFactory PresentationContext::Controls() const {
    return [s = storage_->state](EntityToken entity, GameWorld* world) {
        const auto& b = s->bindings;
        return std::make_unique<ActorControlSession>(
            entity, world,
            ActorControlBindings{b.references, b.lifetime, b.entity, b.actor_control, b.placement, b.arrays});
    };
}
CallResult PresentationContext::FaceEntity(EntityToken token, GameWorld* world, Position3 target, float ratio,
                                           float offset, float duration) const {
    const auto s = storage_->state;
    if (!std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.z) || !std::isfinite(ratio) ||
        !std::isfinite(offset) || !std::isfinite(duration))
        throw std::invalid_argument("facing inputs must be finite");
    const auto current_transform = [s, token, world]() -> void* {
        const auto owned = [&] {
            return !s->closing && world && Alive(token) && !TrackedEntities().IsRetiring(token) &&
                   s->owner.OwnedToken(token.entity).generation == token.generation;
        };
        if (!owned())
            return nullptr;
        const auto actor = s->models->Entities().Read(token.entity);
        if (!owned() || !actor.owner_world.read.returned || actor.owner_world.value != world)
            return nullptr;
        auto* type = s->ComponentType(s->transform_type, detail::ActorComponent::Transform);
        if (!owned())
            return nullptr;
        const auto transform = s->models->Entities().Component(token.entity, type);
        return transform.call.returned && owned() ? transform.object : nullptr;
    };
    auto* transform = current_transform();
    if (!transform)
        throw std::runtime_error("facing requires this client's current owned transform");
    std::vector<ManagedRootId> temporary;
    const auto keep = [&](void* object) {
        temporary.reserve(temporary.size() + 1);
        s->handles.reserve(s->handles.size() + 1);
        const auto id = s->roots.Keep(object);
        temporary.push_back(id);
        s->handles.push_back(id);
        if (!s->roots.Status(id).ready)
            throw std::runtime_error("facing input retention failed");
    };
    const auto release = [&] {
        for (auto id : temporary)
            if (s->roots.Release(id).complete) {
                s->roots.Forget(id);
                const auto found = std::find(s->handles.begin(), s->handles.end(), id);
                if (found != s->handles.end())
                    s->handles.erase(found);
            }
    };
    try {
        keep(token.entity);
        keep(world);
        keep(transform);
        const ActorControl control(s->bindings.actor_control);
        const auto current = control.Yaw(transform);
        const auto pose = s->placement.ReadEntity(token.entity);
        if (!current.read.returned || !std::isfinite(current.value) || !pose.ready)
            throw std::runtime_error("facing origin unreadable");
        const float requested = ResolveFacingYaw(pose.position, target, current.value, ratio, offset);
        // Allocation and native reads can retire or replace the target.
        // Only the current owned Transform is the recipient of this operation.
        if (current_transform() != transform)
            throw std::runtime_error("facing owner or transform changed during preparation");
        const auto result = control.Turn(transform, requested, duration);
        release();
        return result;
    } catch (...) {
        release();
        throw;
    }
}
std::unique_ptr<AnimationEventConstruction> PresentationContext::CreateEventConstructionObserver(
    std::function<void(const AnimationEventSource&)> before,
    std::function<void(const AnimationEventSource&, const AnimationTaskConstruction&)> tasks,
    std::function<void(const AnimationEventSource&, bool)> after) const {
    const auto s = storage_->state;
    if (!s->metadata || !s->isolation)
        throw std::logic_error("event observation requires a prepared isolation context");
    return std::make_unique<AnimationEventConstruction>(
        *s->metadata, s->bindings.references, [s](GameEntity* entity) { return s->isolation->Source(entity); },
        std::move(before), std::move(tasks), std::move(after), s->trace);
}
PresentationContext::PlayerResolver PresentationContext::Player() const {
    return [s = storage_->state](const WorldFrame& frame) { return s->placement.PlayerEntity(frame); };
}
AnimationEventDefinitions PresentationContext::ReadEventDefinitions(const AnimationEventSource& source) const {
    const auto s = storage_->state;
    if (!s->metadata)
        throw std::logic_error("event definitions require prepared metadata");
    return ReadAnimationEventDefinitions(source, *s->metadata, s->bindings.references, s->resources.Arrays(),
                                         s->resources.Components());
}
std::unique_ptr<AnimationSequenceExecutions>
PresentationContext::CreateSequenceObserver(std::function<void(const AnimationSequenceObservation&)> observe) const {
    const auto s = storage_->state;
    if (!s->metadata || !s->isolation)
        throw std::logic_error("sequence observation requires a prepared isolation context");
    return std::make_unique<AnimationSequenceExecutions>(
        *s->metadata, *s->metadata, s->bindings.references,
        [s](const char* ns, const char* type) { return s->metadata->ReadFields(ns, type); },
        [s](GameEntity* entity) { return s->isolation->Source(entity); }, std::move(observe), s->trace);
}
std::unique_ptr<AnimationEventPolicy> PresentationContext::CreateAnimationPolicy(AnimationTaskPolicy policy) const {
    const auto s = storage_->state;
    if (!s->metadata)
        throw std::logic_error("animation policy requires a prepared runtime context");
    auto disabled = detail::DisabledOwnedAnimationTasks(policy, s->isolation_policy);
    std::vector<SelfPerformerRule> performers;
    if (policy.constrain_head_look_to_owner)
        performers.push_back(detail::HeadLookPerformerRule());
    auto& metadata = *s->metadata;
    auto result = std::make_unique<AnimationEventPolicy>(
        metadata, metadata, metadata, metadata,
        [s](const char* ns, const char* type) { return s->metadata->ReadFields(ns, type); }, s->bindings.references,
        [s](GameEntity* entity) { return s->owner.Owns(entity); }, s->trace, std::move(disabled),
        std::move(performers));
    result->SetStringTransform([s](GameEntity* entity, AnimationStringKind kind, const std::string& original) {
        if (kind == AnimationStringKind::AnimationSound)
            try {
                s->audio->Refresh(entity);
            } catch (const std::exception& e) {
                s->Log(e.what());
            }
        const auto skin = s->Skin(entity);
        if (!skin)
            return AnimationStringEdit{};
        auto value = kind == AnimationStringKind::AnimationSound
                         ? std::optional<std::string>(skin->SelectSound(original))
                         : skin->MapResource(original);
        return AnimationStringEdit{!value || *value != original, std::move(value)};
    });
    return result;
}
void PresentationContext::Activate() {
    auto& s = *storage_->state;
    s.audio->Activate();
    if (s.isolation && s.isolation_status.phase != PresentationIsolationPhase::Retired) {
        s.isolation->ActivateOnCurrentThread();
        s.isolation_status.phase = PresentationIsolationPhase::Active;
    }
}
void PresentationContext::Collect() {
    auto& s = *storage_->state;
    if (s.movement_hosts)
        s.movement_hosts->Collect();
    if (s.isolation)
        s.isolation->Collect();
    s.skins->Collect();
    s.audio->Collect();
    s.CollectPendingRoots();
}
bool PresentationContext::Close() {
    auto& s = *storage_->state;
    s.audio->BeginClose();
    s.skins->Collect();
    if (!s.skins->ActorsDrained())
        return false;
    bool closed = s.audio->Close();
    if (!s.CollectPendingRoots())
        closed = false;
    if (!s.skins->Close())
        closed = false;
    if (s.effect_inputs && !s.effect_inputs->Close())
        closed = false;
    if (s.movement_hosts && !s.movement_hosts->Close())
        closed = false;
    if (s.face_materials && !s.face_materials->Close())
        closed = false;
    if (s.animation_parameters && !s.animation_parameters->Close())
        closed = false;
    if (s.actor_timing && !s.actor_timing->Close())
        closed = false;
    if (s.actor_bgm && !s.actor_bgm->Close())
        closed = false;
    if (s.actor_rendering && !s.actor_rendering->Close())
        closed = false;
    if (s.effect_layouts && !s.effect_layouts->Close())
        closed = false;
    if (s.model_presentations && !s.model_presentations->Close())
        closed = false;
    if (s.visual_configurations && !s.visual_configurations->Close())
        closed = false;
    if (s.model_changes && !s.model_changes->Close())
        closed = false;
    s.closing = true;
    if (s.effect_control && !s.effect_control->Close())
        closed = false;
    if (s.hit_effect_control && !s.hit_effect_control->Close())
        closed = false;
    if (s.effect_actions && !s.effect_actions->Close())
        closed = false;
    if (s.effect_parameters && !s.effect_parameters->Close())
        closed = false;
    for (auto it = s.exit_inputs.begin(); it != s.exit_inputs.end();) {
        if ((*it)->Close())
            it = s.exit_inputs.erase(it);
        else {
            closed = false;
            ++it;
        }
    }
    for (auto it = s.handles.begin(); it != s.handles.end();) {
        if (s.roots.Release(*it).complete) {
            s.roots.Forget(*it);
            it = s.handles.erase(it);
        } else {
            closed = false;
            ++it;
        }
    }
    s.event_type = nullptr;
    s.model_type = 0;
    s.style_type = 0;
    if (closed && s.isolation) {
        s.isolation->Retire();
        s.isolation_status.phase = PresentationIsolationPhase::Retired;
    }
    return closed;
}
}
