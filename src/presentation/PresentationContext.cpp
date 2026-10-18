#include <hksr/presentation/PresentationContext.hpp>
#include <hksr/presentation/PresentationBindings.hpp>
#include <hksr/presentation/PresentationResources.hpp>
#include <hksr/presentation/PresentationIsolation.hpp>
#include <hksr/presentation/timeline/OwnedTimeline.hpp>
#include <hksr/runtime/RuntimeHost.hpp>
#include <hksr/entity/EntityOwner.hpp>
#include <hksr/entity/NpcCreationSession.hpp>
#include <hksr/entity/EntityCreationSession.hpp>
#include <hksr/animation/ActorAnimationSession.hpp>
#include <hksr/model/CharacterOutfit.hpp>
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
#include <hksr/presentation/events/AnimationEventConstruction.hpp>
#include <hksr/presentation/events/AnimationSequenceExecutions.hpp>
#include <hksr/presentation/events/AnimationEventDefinitions.hpp>
#include <hksr/presentation/events/NpcEventSupplement.hpp>
#include <hksr/presentation/events/NpcEventBindings.hpp>
#include <hksr/control/ActorControlSession.hpp>
#include <hksr/control/PartnerControlSession.hpp>
#include <hksr/control/ActorControlBindings.hpp>
#include <hksr/control/MouseMode.hpp>
#include <hksr/animation/OfficialCatalog.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
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
    RuntimeMetadata* metadata=nullptr;
    bool prepared=false;
    PresentationIsolationStatus isolation_status;
    Trace trace;
    std::unique_ptr<PresentationIsolation> isolation;
    std::unique_ptr<EffectInputs> effect_inputs;
    std::unique_ptr<EffectControl> effect_control;
    std::unique_ptr<EffectActions> effect_actions;
    std::unique_ptr<EffectAnimatorParameters> effect_parameters;
    std::unique_ptr<EffectTriggerSource> effect_sources;
    bool effect_control_closed=false;
    std::optional<EffectAccess> effect_access;
    EffectCompletionAllocation completion;
    std::vector<std::unique_ptr<AnimationExitConfig>> exit_inputs;
    ManagedRoots roots;
    std::vector<ManagedRootId> handles;
    void* event_type=nullptr;
    void* model_type=nullptr;
    void* style_type=nullptr;

    State(ManagedObjects& input,PresentationBindings value)
        :objects(input),bindings(std::move(value)),resources(bindings),
         models(std::make_shared<ModelServices>(ModelServiceBindings{bindings.references,bindings.lifetime,
             bindings.renderer,bindings.entity,ModelAccess(bindings.model),ArtAdmission(bindings.admission)})),
         map(bindings.world_map),placement(bindings.placement),
         animation(bindings.animation),profile_inputs(bindings.profile_inputs),
         roots(bindings.references){}
    void* ComponentType(void*& cached,const char* name) {
        if(!cached) {
            const auto type=objects.ManagedType("RPG.GameCore",name);
            if(!type.call.returned || !type.object)throw std::runtime_error(std::string(name)+" Type unavailable");
            handles.push_back(0);
            handles.back()=roots.Keep(type.object);
            const auto kept=roots.Target(handles.back());
            if(!kept.call.returned || !kept.object)throw std::runtime_error(std::string(name)+" Type retention failed");
            cached=kept.object;
        }
        return cached;
    }
    void Log(const std::string& text) const noexcept {
        if(trace)try{trace(text);}catch(...){}
    }
    EffectControl& EffectControls() {
        if(effect_control_closed)throw std::logic_error("presentation effect control closed");
        if(!effect_control) {
            const auto methods=ResolveEffectControl460(*metadata);
            for(const auto& error:methods.binding_errors)Log("effect control binding: "+error);
            effect_control=std::make_unique<EffectControl>(*effect_access,methods,bindings.references,objects,
                metadata->ReadFields("RPG.Client","MonoEffect"));
        }
        return *effect_control;
    }
    EffectActions& EffectOperations() {
        if(effect_control_closed)throw std::logic_error("presentation effect actions closed");
        if(!effect_actions) {
            const auto methods=ResolveEffectActions460(*metadata);
            for(const auto& error:methods.binding_errors)Log("effect action binding: "+error);
            effect_actions=std::make_unique<EffectActions>(*effect_access,methods,bindings.references,objects,
                resources.Arrays(),metadata->ReadFields("RPG.Client","MonoEffect"));
        }
        return *effect_actions;
    }
    EffectAnimatorParameters& EffectParameters() {
        if(effect_control_closed)throw std::logic_error("presentation effect parameters closed");
        if(!effect_parameters) {
            const auto methods=ResolveEffectAnimatorParameters460(*metadata);
            for(const auto& error:methods.binding_errors)Log("effect parameter binding: "+error);
            effect_parameters=std::make_unique<EffectAnimatorParameters>(*effect_access,methods,bindings.references,objects,
                resources.Arrays(),metadata->ReadFields("RPG.Client","MonoEffect"));
        }
        return *effect_parameters;
    }
    std::unique_ptr<EventRegistrationLease> ExitEvents(EntityToken entity,const std::string& state,
        const std::vector<EffectRemovalSource>& sources) {
        for(auto it=exit_inputs.begin();it!=exit_inputs.end();) {
            if((*it)->Close())it=exit_inputs.erase(it);else ++it;
        }
        auto config=std::make_unique<AnimationExitConfig>(*metadata,*metadata,objects,*metadata,bindings.references,bindings.arrays);
        auto* input=config.get();exit_inputs.push_back(std::move(config));
        const bool ready=input->Prepare(state,sources);
        for(const auto& error:input->Errors())Log("action exit input: "+error);
        if(!ready)return {};
        if(!event_type) {
            const auto types=ResolveAnimationInputTypes(objects);
            if(!types.events.call.returned || !types.events.object)throw std::runtime_error("action exit event Type unavailable");
            handles.push_back(0);handles.back()=roots.Keep(types.events.object);
            if(!roots.Status(handles.back()).ready)throw std::runtime_error("action exit event Type retention failed");
            event_type=types.events.object;
        }
        const auto component=models->Entities().Component(entity.entity,event_type);
        if(!component.call.returned || !component.object)throw std::runtime_error("action exit event component unavailable");
        auto events=std::make_unique<EventRegistrationLease>(bindings.references,AnimationEvents(bindings.events),ManagedReferenceList(bindings.event_list));
        events->PrepareInline(input->Groups());
        if(!events->Status().prepared)return events;
        events->Attach(entity.entity,component.object,entity);
        input->Close(); // registration now retains the complete private group
        return events;
    }
};
struct PresentationContext::Storage {std::shared_ptr<State> state;};
std::unique_ptr<PresentationContext> PresentationContext::Create(RuntimeMetadata& metadata,RuntimeCapabilities capabilities,
    PresentationIsolationPolicy policy,Trace trace) {
    auto context=std::make_unique<PresentationContext>(metadata,ResolvePresentationBindings(metadata));
    context->Prepare(metadata,capabilities,policy,std::move(trace));return context;
}
PresentationContext::PresentationContext(ManagedObjects& objects,PresentationBindings bindings)
    :storage_(new Storage{std::make_shared<State>(objects,std::move(bindings))}){}
PresentationContext::~PresentationContext(){delete storage_;}
const PresentationBindings& PresentationContext::Bindings() const{return storage_->state->bindings;}
const PresentationResources& PresentationContext::Resources() const{return storage_->state->resources;}
std::shared_ptr<const ModelServices> PresentationContext::Models() const{return storage_->state->models;}
std::unique_ptr<ManagedRoots> PresentationContext::CreateRoots() const {
    return std::make_unique<ManagedRoots>(storage_->state->bindings.references);
}
WorldMapSnapshot PresentationContext::ReadMap(const WorldFrame& frame) const {
    return storage_->state->map.Read(frame);
}
PlayerPlacement PresentationContext::ReadPlayerPlacement(const WorldFrame& frame) const {
    return storage_->state->placement.ReadPlayer(frame);
}
const std::vector<std::string>& PresentationContext::BindingErrors() const{return storage_->state->bindings.errors;}
std::unique_ptr<OfficialCatalog> PresentationContext::CreateCatalog() const {
    const auto& s=*storage_->state;
    if(!s.metadata)throw std::logic_error("official catalog requires a prepared runtime context");
    return std::make_unique<OfficialCatalog>(*s.metadata,*s.metadata,s.objects,s.bindings.references,
        s.resources.Arrays(),s.resources.Components());
}
std::unique_ptr<MouseModeLease> PresentationContext::CreateMouseMode() const {
    const auto& s=*storage_->state;
    if(!s.metadata)throw std::logic_error("mouse mode requires a prepared runtime context");
    const auto methods=ResolveMouseMode(*s.metadata);
    ObserveMouseModeOwners(methods);
    return std::make_unique<MouseModeLease>(methods,s.bindings.references);
}
namespace {
bool Alive(EntityToken token) {
    return token.entity && !TrackedEntities().IsDisposed(token) &&
        TrackedEntities().Current(token.entity).generation==token.generation;
}
}
std::optional<ActorAnimatorView> PresentationContext::ReadAnimator(EntityToken token) const {
    auto& s=*storage_->state;
    if(!Alive(token))return {};
    const auto type=s.ComponentType(s.model_type,"CharacterModelComponent");
    if(!Alive(token))return {};
    const auto component=s.models->Entities().Component(token.entity,type);
    if(!component.call.returned || !component.object || !Alive(token))return {};
    const auto model=s.models->Models().Read(static_cast<CharacterModelComponent*>(component.object));
    if(!Alive(token) || !model.changing.read.returned || model.changing.value ||
        !model.animator.read.returned || !model.animator.value)return {};
    const auto initialized=s.profile_inputs.AnimatorInitialized(model.animator.value);
    if(!initialized.read.returned || !initialized.value || !Alive(token))return {};
    const auto controller=s.animation.Controller(model.animator.value);
    if(!controller.call.returned || !controller.object || !Alive(token))return {};
    return ActorAnimatorView{component.object,model.animator.value,controller.object};
}
void* PresentationContext::ReadEventReceiver(EntityToken token,GameWorld* world) const {
    auto& s=*storage_->state;
    if(!world || !Alive(token))return nullptr;
    const auto entity=s.models->Entities().Read(token.entity);
    if(!Alive(token) || !entity.owner_world.read.returned || entity.owner_world.value!=world ||
        !entity.loaded.read.returned || !entity.loaded.value)return nullptr;
    if(!s.event_type) {
        const auto type=ResolveAnimationInputTypes(s.objects).events;
        if(!type.call.returned || !type.object)throw std::runtime_error("event Type unavailable");
        s.handles.push_back(0);s.handles.back()=s.roots.Keep(type.object);
        const auto kept=s.roots.Target(s.handles.back());
        if(!kept.call.returned || !kept.object)throw std::runtime_error("event Type retention failed");
        s.event_type=kept.object;
    }
    if(!Alive(token))return nullptr;
    const auto component=s.models->Entities().Component(token.entity,s.event_type);
    if(!Alive(token) || !component.call.returned || !component.object)return nullptr;
    const auto receiver=AnimationEvents(s.bindings.events).Consumer(component.object);
    return Alive(token) && receiver.call.returned?receiver.object:nullptr;
}
void* PresentationContext::NpcMotionComponent(EntityToken token,GameWorld* world) const {
    auto& s=*storage_->state;
    if(!Alive(token))throw std::runtime_error("selected character is retiring");
    const auto& entities=s.models->Entities();
    const auto state=entities.Read(token.entity);
    if(!Alive(token))throw std::runtime_error("selected character ended during query");
    if(!state.owner_world.read.returned || state.owner_world.value!=world || !state.loaded.read.returned || !state.loaded.value)
        throw std::runtime_error("selected character is not loaded in the current world");
    const auto type=s.ComponentType(s.style_type,"FreeStyleComponent");
    if(!Alive(token))throw std::runtime_error("selected character ended during query");
    const auto component=entities.Component(token.entity,type);
    if(!component.call.returned || !component.object || !Alive(token))throw std::runtime_error("own FreeStyle component unavailable");
    return component.object;
}
const EntityOwner& PresentationContext::Owner() const{return storage_->state->owner;}
PresentationContext::NpcFactory PresentationContext::Npcs() const {
    return [s=storage_->state,observer=BirthObserver()] {
        const auto& b=s->bindings;
        return std::make_unique<NpcCreationSession>(s->owner,b.factory,b.entity,b.references,observer);
    };
}
PresentationContext::AnimationFactory PresentationContext::Animations() const {
    return [s=storage_->state](EntityToken owner) {
        return std::make_unique<ActorAnimationSession>(owner,s->bindings);
    };
}
PresentationContext::EntityFactory PresentationContext::Entities() const {
    auto s=storage_->state;
    Method story;
    if(s->metadata)story=ResolveStoryActorFactory(*s->metadata);
    return [s,story,observer=BirthObserver()] {
        auto session=std::make_unique<EntityCreationSession>(s->owner,s->bindings.factory,story,&s->objects,
            s->bindings.entity,s->bindings.references,observer);
        if(s->metadata)session->PrepareStoryRuntime(*s->metadata);
        return session;
    };
}
PresentationContext::EntityEventFactory PresentationContext::EntityEvents() const {
    auto s=storage_->state;std::size_t story_offset=0;
    if(s->metadata)for(const auto& f:s->metadata->ReadFields("RPG.GameCore","AdventureCharacterConfig",false))
        if(f.name=="AnimEventConfigListInPerformance" && f.type=="System.String[]")story_offset=f.offset;
    return [s,story_offset](EntityCreationSession& entity,GameWorld* world) {
        const auto& b=s->bindings;const auto& r=s->resources;
        return std::make_unique<NpcEventSupplement>(entity,world,b.references,
            NpcEventBindings{EntityAccess(b.entity),b.adventure_data,r.Config(),AnimationEvents(b.events),
                r.Arrays(),ManagedReferenceList(b.event_list),r.Scene()},story_offset);
    };
}
PresentationContext::EntityFactory PresentationContext::Partners() const {
    auto s=storage_->state;
    Method partner;
    if(s->metadata)partner=ResolvePartnerFactory(*s->metadata);
    return [s,partner,observer=BirthObserver()] {
        return std::make_unique<EntityCreationSession>(s->owner,Method{},Method{},&s->objects,
            s->bindings.entity,s->bindings.references,observer,partner);
    };
}
std::unique_ptr<PartnerControlSession> PresentationContext::PartnerControls(EntityToken partner,EntityToken summoner,GameWorld* world) const {
    auto& s=*storage_->state;
    if(!s.metadata)throw std::logic_error("partner controls require a prepared runtime context");
    auto methods=ResolvePartnerControl460(*s.metadata,s.metadata->ReadFields("RPG.GameCore","GameComponentBase",false),
        s.metadata->ReadFields("","GDBHCNLLPAG",false),s.metadata->ReadFields("","ANACIPLIMDE",false));
    return std::make_unique<PartnerControlSession>(s.owner,partner,summoner,world,methods,s.bindings.references,s.objects);
}
PresentationContext::NpcInputFactory PresentationContext::NpcInputs() const {
    if(!storage_->state->metadata)return {};
    return [s=storage_->state](std::uint32_t id,const char* name,Position3 position,Rotation4 rotation) {
        return MapNpcInputs::Prepare(*s->metadata,s->bindings.references,id,name,position,rotation);
    };
}
PresentationContext::NpcEventFactory PresentationContext::NpcEvents() const {
    return [s=storage_->state](NpcCreationSession& npc,GameWorld* world) {
        const auto& b=s->bindings;
        const auto& r=s->resources;
        return std::make_unique<NpcEventSupplement>(npc,world,b.references,
            NpcEventBindings{EntityAccess(b.entity),b.adventure_data,r.Config(),AnimationEvents(b.events),
                r.Arrays(),ManagedReferenceList(b.event_list),r.Scene()});
    };
}
void PresentationContext::Prepare(RuntimeMetadata& metadata,RuntimeCapabilities capabilities,
    PresentationIsolationPolicy policy,Trace trace) {
    auto& s=*storage_->state;
    if(s.prepared)throw std::logic_error("presentation context already prepared");
    s.prepared=true;s.trace=std::move(trace);
    try {InstallMovementPlacement(s.bindings.actor_control);}
    catch(const std::exception& e){s.Log(std::string("movement placement unavailable: ")+e.what());}
    try {
        if(!capabilities.entity_lifecycle)throw std::runtime_error("entity birth observation unavailable");
        s.isolation=std::make_unique<PresentationIsolation>(metadata,s.bindings.references,policy,s.trace);
        s.isolation_status.phase=PresentationIsolationPhase::Prepared;
        s.Log("presentation isolation prepared; activation precedes owned creation; game qualification pending");
    }catch(const std::exception& e){s.isolation_status={PresentationIsolationPhase::Unavailable,e.what()};s.Log(std::string("presentation isolation unavailable (original behavior retained): ")+e.what());}
    // Version qualification stays inside the SDK. Independent creation and
    // model bindings remain usable when these optional inputs are unavailable.
    if(metadata.Version()!=RuntimeVersion::Hsr460)return;
    s.metadata=&metadata;
    try {
        const auto methods=ResolveEffectAccess460(metadata);
        for(const auto& error:methods.binding_errors)s.Log("effect binding: "+error);
        s.completion=ResolveEffectCompletion460(metadata);
        s.effect_inputs=std::make_unique<EffectInputs>(metadata,metadata,metadata,s.bindings.references);
        s.effect_access.emplace(methods);
    }catch(const std::exception& e){s.Log(std::string("ready effect services unavailable: ")+e.what());}
}
EntityCreationScope::BirthObserver PresentationContext::BirthObserver() const {
    return [s=storage_->state](EntityOrigin origin){if(s->isolation)s->isolation->RememberBirth(origin);};
}
std::function<std::unique_ptr<CharacterOutfit>()> PresentationContext::Outfits() const {
    if(!storage_->state->metadata)return {};
    return [s=storage_->state]{return std::make_unique<CharacterOutfit>(*s->metadata,*s->metadata,s->objects,s->bindings.references,s->bindings.entity);};
}
ModelRequestServices PresentationContext::ModelRequests() const {
    const auto& b=storage_->state->bindings;
    std::function<std::unique_ptr<EventValueLease>()> values;
    if(storage_->state->metadata)values=[s=storage_->state]{return std::make_unique<EventValueInputs>(*s->metadata,*s->metadata,*s->metadata,s->bindings.references);};
    const auto pending=storage_->state->metadata?ResolveModelPending(*storage_->state->metadata):Method{};
    return {ModelProfile(b.profile,pending),ModelAccess(b.model),ProfileInputs(b.profile_inputs),SceneObjects(b.scene),
        AnimationEvents(b.events),ModelLoadCompleted,std::move(values)};
}
ControllerBindingServices PresentationContext::Controllers() const {
    const auto& b=storage_->state->bindings;
    return {AnimationControl(b.animation),EntityAccess(b.entity),SceneObjects(b.scene),ManagedArrays(b.arrays),
        b.model_loader,b.loader_model,b.loader_manager};
}
PresentationContext::BaselineFactory PresentationContext::Baselines() const {
    return [s=storage_->state,requests=ModelRequests()](EntityToken entity,GameWorld* world,const ProfileDescription& profile) {
        const auto& b=s->bindings;
        auto capture=std::make_unique<ModelBaselineCapture>(entity,world,b.references,EntityAccess(b.entity),requests,b.profile_events,profile);
        if(s->metadata)capture->SetVisibilityLease(std::make_unique<ModelVisibilityLease>(*s->metadata,s->objects,b.references,s->resources.Arrays()));
        return capture;
    };
}
PresentationContext::ModelTransitionFactory PresentationContext::ModelTransitions() const {
    return [s=storage_->state,controllers=Controllers()](EntityToken entity,GameWorld* world,std::unique_ptr<ModelProfileRequest> request) {
        const auto& b=s->bindings;
        return std::make_unique<ModelTransition>(entity,world,std::move(request),
            ModelTransitionServices{b.references,EntityAccess(b.entity),ModelAccess(b.model),AnimationControl(b.animation),controllers,b.profile_events});
    };
}
std::optional<EffectRequestServices> PresentationContext::Effects() const {
    const auto s=storage_->state;
    if(!s->effect_access)return {};
    return EffectRequestServices{s->bindings.references,*s->effect_access,s->completion,
        [s]{return s->effect_inputs->Manager();},
        [s](void* input,void* target,void* callback){return s->effect_inputs->Configure(input,target,callback);},s->trace,
        [s](EffectReference ref,void* entity){if(s->isolation)s->isolation->RememberRequest(ref,static_cast<GameEntity*>(entity));},
        [s](EffectReference ref){if(s->isolation)s->isolation->RetireRequest(ref);},
        [s](EntityToken entity,const std::string& state,const std::vector<EffectRemovalSource>& sources){return s->ExitEvents(entity,state,sources);},
        [s](void* parameters,void* caster,void* callback,const SpatialEffectOptions& options) {
            return s->effect_inputs->ConfigureSpatial(parameters,caster,callback,options);
        }};
}
std::shared_ptr<const PresentationEffects> PresentationContext::EffectServices() const {
    auto services=Effects();
    if(!services)return {};
    const auto s=storage_->state;
    EffectControlServices control;
    control.progress=[s](EffectReference ref,float value,float duration){return s->EffectControls().SetProgress(ref,value,duration);};
    control.animator=[s](EffectReference ref,int hash,float duration,bool restart){return s->EffectControls().SetAnimatorState(ref,hash,duration,restart);};
    control.named_animator=[s](EffectReference ref,const std::string& name,float duration,bool restart){return s->EffectControls().SetAnimatorState(ref,name,duration,restart);};
    EffectActionServices actions;
    actions.trigger=[s](EffectReference ref,const std::string& name,bool reset){return s->EffectOperations().Trigger(ref,name,reset);};
    actions.remove=[s](void* manager,EffectReference ref,const EffectRemovalOptions& options){return s->EffectOperations().Remove(manager,ref,options);};
    actions.parameter=[s](EffectReference ref,const EffectAnimatorParameter& value){return s->EffectParameters().Set(ref,value);};
    return std::make_shared<PresentationEffects>(std::move(*services),std::move(control),
        [s](void* parameters,void* caster,void* target,void* callback,const EntityEffectOptions& options) {
            return s->effect_inputs->ConfigureEntity(parameters,caster,target,callback,options);
        },std::move(actions));
}
PresentationIsolationStatus PresentationContext::IsolationStatus() const{return storage_->state->isolation_status;}
std::unique_ptr<EffectEntityFollow> PresentationContext::CreateEffectEntityFollow() const {
    const auto s=storage_->state;
    if(!s->metadata || !s->effect_access || s->effect_control_closed)
        throw std::logic_error("presentation effects are not prepared or are closed");
    auto& metadata=*s->metadata;
    const auto behavior=metadata.FindType("","GIMGIMFGBFF");
    const auto observed=metadata.ObserveFields(behavior);
    RuntimeType list_type;
    for(const auto& field:observed.fields)if(field.declaration.name=="PJFPEKPDKIH") {
        if(!field.error.empty())throw std::runtime_error(field.error);
        if(list_type)throw std::runtime_error("follow list field is ambiguous");
        list_type=metadata.FieldType(field.field);
    }
    if(!list_type)throw std::runtime_error("follow list declaration unavailable: "+observed.iteration_error);
    auto methods=ResolveEffectEntityFollow460(metadata);
    const auto fields=ResolveEffectEntityFollowFields460(metadata.ReadFields("RPG.Client","MonoEffect"),
        metadata.ReadFields("RPG.Client","MonoEffectPluginEntityFollowEffect"),metadata.ReadFields(behavior));
    auto list=ResolveReferenceListMethods(metadata.ReadType(list_type),"RPG.GameCore.GameEntity");
    for(const auto& error:methods.binding_errors)s->Log("effect follow binding: "+error);
    for(const auto& error:list.binding_errors)s->Log("effect follow list: "+error);
    return std::make_unique<EffectEntityFollow>(*s->effect_access,std::move(methods),fields,s->bindings.references,
        s->objects,s->resources.Arrays(),ManagedReferenceList(std::move(list)),
        ResolveEffectMotionTarget460(metadata,metadata.ReadFields("","BGOBCKLPEMK")));
}
std::unique_ptr<OwnedTimeline> PresentationContext::CreateTimeline(EntityToken token,GameWorld* world) const {
    const auto s=storage_->state;
    if(!s->isolation)throw std::logic_error("Timeline playback requires presentation isolation");
    return s->isolation->CreateTimeline({token,world});
}
TextResult PresentationContext::ReadEffectTriggerPath(const EffectTriggerExecution& trigger) const {
    auto& s=*storage_->state;
    if(!s.metadata)throw std::logic_error("effect source reading requires prepared metadata");
    if(!s.effect_sources)s.effect_sources=std::make_unique<EffectTriggerSource>(
        s.metadata->ReadFields("RPG.GameCore","TriggerEffect"),s.bindings.references,s.resources.Components());
    return s.effect_sources->Path(trigger);
}
std::unique_ptr<EffectSubmissions> PresentationContext::CreateEffectObserver(
    std::function<void(const EffectSubmission&)> before,
    std::function<void(const EffectSubmission&,bool)> after) const {
    const auto s=storage_->state;
    if(!s->metadata || !s->isolation)throw std::logic_error("effect observation requires a prepared isolation context");
    return std::make_unique<EffectSubmissions>(*s->metadata,s->bindings.references,
        [s](const char* ns,const char* type){return s->metadata->ReadFields(ns,type);},
        [s](GameEntity* entity){return s->isolation->Source(entity);},std::move(before),std::move(after),s->trace);
}
PresentationContext::ControlFactory PresentationContext::Controls() const {
    return [s=storage_->state](EntityToken entity,GameWorld* world) {
        const auto& b=s->bindings;
        return std::make_unique<ActorControlSession>(entity,world,
            ActorControlBindings{b.references,b.lifetime,b.entity,b.actor_control,b.placement,b.arrays});
    };
}
std::unique_ptr<AnimationEventConstruction> PresentationContext::CreateEventConstructionObserver(
    std::function<void(const AnimationEventSource&)> before,
    std::function<void(const AnimationEventSource&,const AnimationTaskConstruction&)> tasks,
    std::function<void(const AnimationEventSource&,bool)> after) const {
    const auto s=storage_->state;
    if(!s->metadata || !s->isolation)throw std::logic_error("event observation requires a prepared isolation context");
    return std::make_unique<AnimationEventConstruction>(*s->metadata,s->bindings.references,
        [s](GameEntity* entity){return s->isolation->Source(entity);},std::move(before),std::move(tasks),std::move(after),s->trace);
}
PresentationContext::PlayerResolver PresentationContext::Player() const {
    return [s=storage_->state](const WorldFrame& frame){return s->placement.PlayerEntity(frame);};
}
AnimationEventDefinitions PresentationContext::ReadEventDefinitions(const AnimationEventSource& source) const {
    const auto s=storage_->state;
    if(!s->metadata)throw std::logic_error("event definitions require prepared metadata");
    return ReadAnimationEventDefinitions(source,*s->metadata,s->bindings.references,s->resources.Arrays(),s->resources.Components());
}
std::unique_ptr<AnimationSequenceExecutions> PresentationContext::CreateSequenceObserver(
    std::function<void(const AnimationSequenceObservation&)> observe) const {
    const auto s=storage_->state;
    if(!s->metadata || !s->isolation)throw std::logic_error("sequence observation requires a prepared isolation context");
    return std::make_unique<AnimationSequenceExecutions>(*s->metadata,*s->metadata,s->bindings.references,
        [s](const char* ns,const char* type){return s->metadata->ReadFields(ns,type);},
        [s](GameEntity* entity){return s->isolation->Source(entity);},std::move(observe),s->trace);
}
std::unique_ptr<AnimationEventPolicy> PresentationContext::CreateAnimationPolicy(AnimationTaskPolicy policy) const {
    const auto s=storage_->state;
    if(!s->metadata)throw std::logic_error("animation policy requires a prepared runtime context");
    std::vector<std::string> disabled;
    std::vector<SelfPerformerRule> performers;
    if(!policy.allow_camera_noise)disabled.emplace_back("RPG.GameCore.VCameraNoiseChange");
    if(policy.constrain_head_look_to_owner)performers.push_back({"RPG.GameCore.HeadLookAt","PerformerType"});
    auto& metadata=*s->metadata;
    return std::make_unique<AnimationEventPolicy>(metadata,metadata,metadata,metadata,
        [s](const char* ns,const char* type){return s->metadata->ReadFields(ns,type);},s->bindings.references,
        [s](GameEntity* entity){return s->owner.Owns(entity);},s->trace,std::move(disabled),std::move(performers));
}
void PresentationContext::Activate(){auto& s=*storage_->state;if(s.isolation && s.isolation_status.phase!=PresentationIsolationPhase::Retired){s.isolation->ActivateOnCurrentThread();s.isolation_status.phase=PresentationIsolationPhase::Active;}}
void PresentationContext::Collect(){if(storage_->state->isolation)storage_->state->isolation->Collect();}
bool PresentationContext::Close() {
    auto& s=*storage_->state;
    bool closed=!s.effect_inputs || s.effect_inputs->Close();
    s.effect_control_closed=true;
    if(s.effect_control && !s.effect_control->Close())closed=false;
    if(s.effect_actions && !s.effect_actions->Close())closed=false;
    if(s.effect_parameters && !s.effect_parameters->Close())closed=false;
    for(auto it=s.exit_inputs.begin();it!=s.exit_inputs.end();) {
        if((*it)->Close())it=s.exit_inputs.erase(it);else {closed=false;++it;}
    }
    for(auto it=s.handles.begin();it!=s.handles.end();) {
        if(s.roots.Release(*it).complete){s.roots.Forget(*it);it=s.handles.erase(it);}else {closed=false;++it;}
    }
    s.event_type=nullptr;
    s.model_type=0;s.style_type=0;
    if(closed && s.isolation){s.isolation->Retire();s.isolation_status.phase=PresentationIsolationPhase::Retired;}
    return closed;
}
}
