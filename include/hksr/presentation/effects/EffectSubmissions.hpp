#pragma once
#include "EffectAccess.hpp"
#include "../../entity/EntityLifecycle.hpp"
#include "../../metadata/RuntimeMetadata.hpp"
#include <functional>

namespace hksr::sdk_v1 {
enum class EffectSubmissionKind { Synchronous, Asynchronous };
// A TriggerEffect.OnTaskBegin invocation, not an animation-state or group ID.
// Zero means no qualified TriggerEffect caller. Pointers are borrowed only
// during observer callbacks; retain them explicitly if needed afterwards.
struct EffectTriggerExecution {
    std::uint64_t serial=0;
    void* task=nullptr;
    void* config=nullptr;
    void* context=nullptr;
};
struct EffectSubmission {
    std::uint64_t serial=0;
    EntityOrigin origin;
    void* manager=nullptr;
    EffectReference request;
    EffectSubmissionKind kind=EffectSubmissionKind::Asynchronous;
    EffectTriggerExecution trigger;
};
// Observe official submissions without changing parameters, callbacks, return
// values or exceptions. Install before creation and activate on the engine
// thread. Identify must return an actual owned entity generation, or empty.
// A TargetEntity is never used as a substitute for CasterEntity.
class HKSR_API EffectSubmissions {
public:
    using Fields=std::function<std::vector<DeclaredField>(const char*,const char*)>;
    using Identify=std::function<EntityOrigin(GameEntity*)>;
    using Before=std::function<void(const EffectSubmission&)>;
    using After=std::function<void(const EffectSubmission&,bool original_returned)>;
    using Trace=std::function<void(const std::string&)>;
    EffectSubmissions(MethodResolver&,ReferenceMethods,Fields,Identify,Before,After,Trace={});
    ~EffectSubmissions();
    EffectSubmissions(const EffectSubmissions&)=delete;
    EffectSubmissions& operator=(const EffectSubmissions&)=delete;
    void ActivateOnCurrentThread();
    void Stop() noexcept;
private:
    struct State;struct Holder;Holder* holder_=nullptr;
};
// Before runs with retained manager/request/source inputs BEFORE the original
// submission; use it to retain the receipt, e.g. TrackSubmitted. After reports
// the original call outcome, not loading success. Neither callback is a safe
// point for removing effects: service cancellation after the producing engine
// stack returns. Async->sync forwarding of the same request is reported once.
// Direct submissions can have an empty trigger. No action ownership is inferred
// from menu selection, animation state, task address, or completion-time state.
}
