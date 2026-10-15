#pragma once
#include "../animation/ControllerBinding.hpp"
#include "ModelProfileRequest.hpp"
#include <optional>

namespace hksr::sdk_v1 {
enum class ModelTransitionPhase { Ready, Detaching, Loading, Adopting, Binding, Initializing, Failed, Closing, Closed };
struct ModelTransitionServices {
    ReferenceMethods references;
    EntityAccess entities;
    ModelAccess models;
    AnimationControl animation;
    ControllerBindingServices controllers;
    ProfileEventFields event_fields;
};
struct ModelTransitionPlan {
    ModelDestination destination=ModelDestination::Baseline;
    std::optional<ControllerSource> controller;
    bool reload=true,require_animator=true,initialize=false;
};
enum class InitialOutputPhase { Pending, Returned, Absent, Failed };
struct InitialOutput { InitialOutputPhase phase=InitialOutputPhase::Pending; int hash=0; };
// Synchronous extension points for consumer-owned overlays and initial action
// policy. Not retained. Return false only while that consumer is still draining
// or adopting. Empty callbacks mean no additional consumer work. The SDK owns
// the order, model/controller/event operations, and official output boundary.
struct ModelTransitionConsumer {
    std::function<bool()> detach,adopt;
    std::function<void()> events_ready;
    std::function<InitialOutput()> initialize;
    std::function<bool()> interrupted;
};
struct ModelTransitionStatus {
    ModelTransitionPhase phase=ModelTransitionPhase::Ready;
    bool controller_ready=false,closed=false;
    std::string error,initialization_note,event_value_note;
    AnimatorStateObservation initial_state;
};
// Engine transaction for one owned entity, independent of UI, action domains,
// external-asset types and entity destruction policy. Select/Cancel are intent
// operations on the owning thread (including synchronous callbacks), not a
// cross-thread queue. Service advances all available stages in one delivery.
// Close cancels before entity retirement; call again after retirement to drain
// retained event inputs. No automatic restoration or destruction on failure.
class HKSR_API ModelTransition {
public:
    ModelTransition(EntityToken,GameWorld*,std::unique_ptr<ModelProfileRequest>,ModelTransitionServices);
    ~ModelTransition(); // no engine calls; explicit Close must finish first
    ModelTransition(const ModelTransition&)=delete;
    ModelTransition& operator=(const ModelTransition&)=delete;
    void Select(ModelTransitionPlan);
    void Cancel();
    void Service(const WorldFrame&,ManagedObjects&,AnimationInputTypes,const ModelTransitionConsumer& = {});
    bool Close(const WorldFrame&);
    const ModelTransitionStatus& Status() const;
    const ModelRequestStatus* RequestStatus() const;
private:
    struct State;State* state_;
};
}
