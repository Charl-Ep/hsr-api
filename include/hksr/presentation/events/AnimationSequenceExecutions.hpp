#pragma once
#include "AnimationEventConstruction.hpp"

namespace hksr::sdk_v1 {
enum class AnimationSequenceKind { Persistent, Immediate };
enum class AnimationSequenceOperation { Begin, Tick, Reset, Dispose };
struct AnimationSequenceExecution {
    std::uint64_t construction=0,definition=0,serial=0,parent_serial=0;
    EntityOrigin origin;
    void* sequence=nullptr;
    void* context=nullptr;
    AnimationSequenceKind kind=AnimationSequenceKind::Persistent;
};
struct AnimationSequenceObservation {
    AnimationSequenceExecution execution;
    AnimationSequenceOperation operation=AnimationSequenceOperation::Begin;
    bool entering=false,original_returned=false,state_read=false,continues=false;
    std::int32_t task_state=0;
};
enum class AnimationSequenceTracking { Tracked, Unsupported, Unavailable };
// Observes already-constructed event sequences; never starts or ticks them.
// Pass actual task-construction receipts to Track on the active owner thread.
// Definitions are weak references. Begin creates an execution ID, persistent
// Tick retains it, and terminal return/reset/dispose ends it. Immediate tasks
// are scoped to Begin: their throwing Tick is not used as a progress entry.
class HKSR_API AnimationSequenceExecutions {
public:
    using Fields=std::function<std::vector<DeclaredField>(const char*,const char*)>;
    using Identify=std::function<EntityOrigin(GameEntity*)>;
    using Observe=std::function<void(const AnimationSequenceObservation&)>;
    using Trace=std::function<void(const std::string&)>;
    AnimationSequenceExecutions(MethodResolver&,ObjectMetadata&,ReferenceMethods,Fields,Identify,Observe,Trace={});
    ~AnimationSequenceExecutions();
    AnimationSequenceExecutions(const AnimationSequenceExecutions&)=delete;
    AnimationSequenceExecutions& operator=(const AnimationSequenceExecutions&)=delete;
    void ActivateOnCurrentThread();
    AnimationSequenceTracking Track(const AnimationEventSource&,const AnimationTaskConstruction&);
    void Collect(); // release collected definitions, not game objects
    void Stop() noexcept; // detach callbacks; does not release managed handles
    bool Close(); // Stop + release weak handles on the owning thread
private:
    struct State;struct Holder;Holder* holder_=nullptr;
};
// References in notifications are borrowed for that call only. A definition
// does not keep a sequence alive. Missing Begin is never inferred from Tick.
// Close before the metadata domain ends. Destruction performs no engine calls.
}
