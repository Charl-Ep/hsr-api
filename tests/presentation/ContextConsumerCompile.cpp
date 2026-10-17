#include <hksr/presentation/PresentationContext.hpp>
#include <hksr/presentation/PresentationResources.hpp>
#include <hksr/control/ActorControlSession.hpp>
#include <hksr/animation/ActorAnimationSession.hpp>
#include <hksr/entity/NpcCreationSession.hpp>
#include <hksr/entity/EntityCreationSession.hpp>
#include <hksr/presentation/events/NpcEventSupplement.hpp>
#include <hksr/model/ModelBaselineCapture.hpp>
#include <hksr/model/ModelTransition.hpp>
#include <hksr/model/ModelServices.hpp>
#include <hksr/control/LookSupport.hpp>
#include <hksr/control/MouseMode.hpp>
#include <hksr/animation/OfficialCatalog.hpp>
#include <hksr/world/WorldMap.hpp>
#include <hksr/world/WorldPlacement.hpp>
#include <hksr/presentation/events/AnimationEventPolicy.hpp>
#include <hksr/presentation/events/AnimationEventConstruction.hpp>
#include <hksr/presentation/events/AnimationEventDefinitions.hpp>
#include <hksr/presentation/effects/PresentationEffects.hpp>
#include <hksr/presentation/effects/EffectEntityFollow.hpp>
#include <hksr/presentation/effects/EffectSubmissions.hpp>
#include <hksr/runtime/RuntimeHost.hpp>

// Compile-only ordinary consumer. It does not know the binding table or any
// concrete detour provider; no game call occurs in this build test.
class ContextConsumer final:public hksr::api::RuntimeClient {
    std::unique_ptr<hksr::api::PresentationContext> context_;
    std::unique_ptr<hksr::api::AnimationEventPolicy> policy_;
    std::unique_ptr<hksr::api::EffectSubmissions> effects_;
    std::unique_ptr<hksr::api::AnimationEventConstruction> event_sources_;
    std::unique_ptr<hksr::api::AnimationSequenceExecutions> sequences_;
public:
    void Prepare(hksr::api::RuntimeMetadata& metadata,hksr::api::RuntimeCapabilities capabilities) override {
        context_=hksr::api::PresentationContext::Create(metadata,capabilities,{});
        policy_=context_->CreateAnimationPolicy({});
        effects_=context_->CreateEffectObserver([](const auto&){},[](const auto&,bool){});
        sequences_=context_->CreateSequenceObserver([](const auto&){});
        event_sources_=context_->CreateEventConstructionObserver([this](const auto& source){
                const auto definitions=context_->ReadEventDefinitions(source);(void)definitions;
            },
            [this](const auto& source,const auto& tasks){sequences_->Track(source,tasks);},[](const auto&,bool){});
    }
    std::unique_ptr<hksr::api::ActorControlSession> Controls(hksr::api::EntityToken owner,hksr::api::GameWorld* world) {
        return context_->Controls()(owner,world);
    }
    const hksr::api::MeshAccess& Mesh() const {return context_->Resources().Mesh();}
    std::unique_ptr<hksr::api::NpcCreationSession> Npc() {return context_->Npcs()();}
    std::unique_ptr<hksr::api::EntityCreationSession> Partner() {return context_->Partners()();}
    std::unique_ptr<hksr::api::ManagedRoots> ModelRoots() {return context_->Models()->CreateRoots();}
    std::unique_ptr<hksr::api::LookSupport> Look() {return context_->Models()->CreateLookSupport();}
    std::unique_ptr<hksr::api::MapNpcInputs> NpcInputs(std::uint32_t id) {
        return context_->NpcInputs()(id,"consumer",{},{});
    }
    std::unique_ptr<hksr::api::OfficialCatalog> Catalog() {return context_->CreateCatalog();}
    std::unique_ptr<hksr::api::MouseModeLease> Mouse() {return context_->CreateMouseMode();}
    hksr::api::WorldMapSnapshot Map(const hksr::api::WorldFrame& frame) {return context_->ReadMap(frame);}
    hksr::api::PlayerPlacement Placement(const hksr::api::WorldFrame& frame) {return context_->ReadPlayerPlacement(frame);}
    std::unique_ptr<hksr::api::NpcEventSupplement> Events(hksr::api::NpcCreationSession& npc,hksr::api::GameWorld* world) {
        return context_->NpcEvents()(npc,world);
    }
    std::unique_ptr<hksr::api::ActorAnimationSession> Animation(hksr::api::EntityToken owner) {
        return context_->Animations()(owner);
    }
    std::shared_ptr<const hksr::api::PresentationEffects> Effects() {return context_->EffectServices();}
    std::unique_ptr<hksr::api::EffectEntityFollow> EffectFollow() {return context_->CreateEffectEntityFollow();}
    hksr::api::EffectControlResult EffectProgress(hksr::api::EffectReference ref) {
        return context_->EffectServices()->SetProgress(ref,0.5f,0.1f);
    }
    std::optional<hksr::api::ActorAnimatorView> Animator(hksr::api::EntityToken owner) const {
        return context_->ReadAnimator(owner);
    }
    std::unique_ptr<hksr::api::ModelBaselineCapture> Baseline(hksr::api::EntityToken owner,
        hksr::api::GameWorld* world,const hksr::api::ProfileDescription& target) {
        return context_->Baselines()(owner,world,target);
    }
    std::unique_ptr<hksr::api::ModelTransition> Transition(hksr::api::EntityToken owner,
        hksr::api::GameWorld* world,std::unique_ptr<hksr::api::ModelProfileRequest> request) {
        return context_->ModelTransitions()(owner,world,std::move(request));
    }
    bool Frame(const hksr::api::WorldFrame&,bool stop) override {
        context_->Activate();
        if(stop){effects_->Stop();event_sources_->Stop();policy_->Stop();return sequences_->Close() && context_->Close();}
        effects_->ActivateOnCurrentThread();
        sequences_->ActivateOnCurrentThread();
        event_sources_->ActivateOnCurrentThread();
        context_->Collect();return false;
    }
};
