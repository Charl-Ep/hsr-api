#pragma once
#include "ModelProfileInputs.hpp"
#include <hksr/presentation/events/AnimationEvents.hpp>
#include <hksr/presentation/events/EventValueInputs.hpp>
#include <functional>
#include <hksr/entity/EntityAccess.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/unity/SceneObjects.hpp>
#include <hksr/world/WorldFrames.hpp>
#include <memory>

namespace hksr::sdk_v1 {
enum class ModelDestination { Baseline, Alternate };
enum class ModelRequestPhase { Idle, AwaitingModel, ModelReady, EventsReady, Cancelled, Failed, OwnerEnded };
struct ModelRequestStatus {
    std::uint64_t serial=0;
    ModelDestination destination=ModelDestination::Baseline;
    ModelRequestPhase phase=ModelRequestPhase::Idle;
    ChangeResult resolution;
    CallResult submit,cancel,event_replace;
    ModelSnapshot observation;
    Observed<bool> completed,animator_initialized;
    bool operation_in_use=false,cancel_attempted=false,events_attempted=false,closed=false;
    std::vector<std::string> errors;
    std::string event_value_note;
};
struct ModelRequestServices {
    ModelProfile profile;
    ModelAccess model;
    ProfileInputs inputs;
    SceneObjects strings;
    AnimationEvents events;
    Observed<bool> (*completion)(const ModelSnapshot&)=nullptr;
    std::function<std::unique_ptr<EventValueLease>()> event_values;
};
// Actual official operation on one owned entity/model epoch. It does NOT stop
// actions, retire private Art, or adopt payloads. The coordinator must do that
// BEFORE Submit and BETWEEN ModelReady and BindEvents. No auto-restore on error,
// no synthetic completion callbacks/timers, no implicit entity destruction.
// All calls are on the engine thread. Keep alive until Close completes.
class HKSR_API ModelProfileRequest {
public:
    ModelProfileRequest(EntityToken owner,GameWorld* world,
        std::unique_ptr<ModelProfileInputs> inputs,ReferenceMethods refs,ModelRequestServices services);
    ~ModelProfileRequest();
    ModelProfileRequest(const ModelProfileRequest&) = delete;
    ModelProfileRequest& operator=(const ModelProfileRequest&) = delete;
    void Submit(const WorldFrame& frame,CharacterModelComponent* current_model,ModelDestination destination);
    void Observe(const WorldFrame& frame,CharacterModelComponent* current_model,bool require_animator);
    // Use the current model's actual event component and consumer declarations,
    // not those captured before model callbacks reset its event inputs.
    void BindEvents(const WorldFrame& frame,CharacterModelComponent* current_model,
        void* current_event_component,ProfileEventFields fields);
    bool Cancel(const WorldFrame& frame);
    ModelProfileResult Close(const WorldFrame& frame);
    const ModelRequestStatus& Status() const;
private:
    struct State;
    State* state_;
};
}
