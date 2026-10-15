#pragma once
#include "TimelineRegistryAccess.hpp"
#include <hksr/entity/EntityLifecycle.hpp>

namespace hksr::sdk_v1::detail {
enum class TimelineTask {
    Play, Stop, Wait, WaitFinish, EffectSync,
    EffectConfigGFJHLACJMNK, EffectConfigNGLKHNKNBDC,
    HideEffectAutoLayout, StartEffectAim, SetEffectAutoLayoutScale,
    TransferMahJongLayoutEffect, TriggerAutoLayoutTransferPerform,
    SetCharacterEffectMaterialBlock, RecordAutoLayoutSnapShot,
    SetEffectFrameCaptureMatTex, StopEffectFollow, Count
};
enum class TimelineTaskStep { Begin, Tick, Reset, Dispose, Skip };
struct TimelineTaskCall {
    TimelineTask task;
    TimelineTaskStep step;
    void* object=nullptr;
    float delta=0;
};
// Recognized task invocations establish a source barrier, INCLUDING official
// tasks and failed reads. This does not replace the official task scheduler.
// The shared SDK service installs inactive detours, then activates on its
// official owner thread before client-owned creation.
class HKSR_API TimelineTasks {
public:
    using Identify=std::function<EntityOrigin(GameEntity*)>;
    using Trace=std::function<void(const std::string&)>;
    // Before/After run inside the established task scope (or unknown barrier).
    // The receipt supports wait tracking without coupling it to native ABI code.
    using Before=std::function<void*(const TimelineTaskCall&)>;
    using After=std::function<void(void*,const TimelineTaskCall&,bool abnormal)>;
    // Identify returns creation provenance INCLUDING disposed entities, or
    // empty for established non-owned entities. Not a live OwnedToken lookup.
    // Throw when coverage/attribution is unknown; do not query a retired world.
    TimelineTasks(MethodResolver&,ReferenceMethods,TimelineRegistryAccess::Fields,
        Identify,Before,After,Trace);
    ~TimelineTasks();
    TimelineTasks(const TimelineTasks&)=delete;
    TimelineTasks& operator=(const TimelineTasks&)=delete;
    void ActivateOnCurrentThread();
    void Collect();
    bool HasSource(TimelineOwner) const;
    bool Close(); // engine thread, outside task invocation, before Stop
    void Stop() noexcept;
private:
    struct State;struct Holder;Holder* holder_=nullptr;
};
}
