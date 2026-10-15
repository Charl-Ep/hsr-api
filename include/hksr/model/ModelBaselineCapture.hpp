#pragma once
#include "ModelProfileRequest.hpp"

namespace hksr::sdk_v1 {
enum class BaselineCapturePhase { Waiting, Ready, Transferred, Failed, Cancelled, OwnerEnded };
struct BaselineCaptureStatus {
    BaselineCapturePhase phase=BaselineCapturePhase::Waiting;
    bool attempted=false,closed=false;
    std::string waiting;
    std::vector<std::string> errors;
};
// Captures one factory-created model baseline before any consumer overlays its
// Art or event inputs. Waits for the official model/event consumer, roots the
// original arrays, and produces a ready-to-use ModelProfileRequest. It NEVER
// submits a model change, retires an entity, or chooses an animation domain.
// Service/TakeRequest/Close: owning engine thread. Cancel: reentrant intent,
// not a cross-thread API. Close also drains partial preparation after failure.
class HKSR_API ModelBaselineCapture {
public:
    ModelBaselineCapture(EntityToken,GameWorld*,ReferenceMethods,EntityAccess,
        ModelRequestServices,ProfileEventFields,ProfileDescription);
    ~ModelBaselineCapture(); // native destruction only; explicit Close required
    void SetVisibilityLease(std::unique_ptr<ModelVisibilityLease>);
    ModelBaselineCapture(const ModelBaselineCapture&)=delete;
    ModelBaselineCapture& operator=(const ModelBaselineCapture&)=delete;
    void Service(const WorldFrame&,ManagedObjects&,const ManagedArrays&,AnimationInputTypes);
    void Cancel();
    std::unique_ptr<ModelProfileRequest> TakeRequest();
    bool Close();
    const BaselineCaptureStatus& Status() const;
private:
    struct State;State* state_;
};
}
