#pragma once
#include "../../entity/EntityLifecycle.hpp"
#include "../../metadata/RuntimeMetadata.hpp"
#include <functional>

namespace hksr::sdk_v1 {
struct AnimationEventSource {
    std::uint64_t serial=0;
    EntityOrigin origin;
    void* receiver=nullptr;
    void* events=nullptr;
    void* behaviors=nullptr;
    void* groups=nullptr;
    void* logic=nullptr;
    void* context=nullptr;
    void* registration=nullptr;
    void* file=nullptr;
};
struct AnimationTaskConstruction {
    std::uint64_t serial=0;
    // Actual task-list factory inputs and its returned executor array. This
    // is not an index-wise config->task mapping: disabled/failed tasks skip.
    // A nested context may differ from AnimationEventSource.context. The source
    // origin identifies the enclosing construction, not ownership of children.
    void* context=nullptr;
    void* configurations=nullptr;
    void* parent=nullptr;
    void* tasks=nullptr;
    bool original_returned=false;
};
// Shares the construction entry with AnimationEventPolicy. Observes inputs
// AFTER all active private projections, including inline/baseline registrations
// with null file/registration. Does not execute tasks or alter scheduling.
class HKSR_API AnimationEventConstruction {
public:
    using Identify=std::function<EntityOrigin(GameEntity*)>;
    using Before=std::function<void(const AnimationEventSource&)>;
    using Tasks=std::function<void(const AnimationEventSource&,const AnimationTaskConstruction&)>;
    using After=std::function<void(const AnimationEventSource&,bool original_returned)>;
    using Trace=std::function<void(const std::string&)>;
    AnimationEventConstruction(MethodResolver&,ReferenceMethods,Identify,Before,Tasks,After,Trace={});
    ~AnimationEventConstruction();
    AnimationEventConstruction(const AnimationEventConstruction&)=delete;
    AnimationEventConstruction& operator=(const AnimationEventConstruction&)=delete;
    void ActivateOnCurrentThread();
    void Stop() noexcept;
private:
    struct State;struct Holder;Holder* holder_=nullptr;
};
// Pointers are retained only for their callback scopes. Retain explicitly for
// later use. Tasks observes the core array builder, including ordinary List
// construction delegating to it. No result means no successful array observed;
// it is not proof that the whole configuration produced no executors.
// Construction serials are not playback IDs. Stop detaches observations only;
// it neither removes game events nor hot-unloads process hooks.
}
