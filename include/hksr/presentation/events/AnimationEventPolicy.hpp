#pragma once
#include "../../metadata/RuntimeMetadata.hpp"
#include "../../entity/EntityFactory.hpp"
#include <functional>
#include <memory>

namespace hksr::sdk_v1 {
struct SelfPerformerRule {std::string task_type,performer_field;};
// Construction-time configuration projection; official event scheduling stays
// intact. Exact disabled configuration types must have independently audited
// semantics. This is not a complete camera/lighting/Timeline sandbox.
// Policies coexist behind one process hook. Every policy checks its own thread
// and ownership predicate; stopping one does not disable other clients.
// Overlapping ownership composes private projections in registration order.
class HKSR_API AnimationEventPolicy {
public:
    using Owns=std::function<bool(GameEntity*)>;
    using Trace=std::function<void(const std::string&)>;
    using Fields=std::function<std::vector<DeclaredField>(const char*,const char*)>;
    AnimationEventPolicy(MethodResolver&,ObjectMetadata&,ManagedObjects&,TypeRelationships&,Fields,
        ReferenceMethods,Owns,Trace,std::vector<std::string> disabled_config_types,
        std::vector<SelfPerformerRule> self_performers={});
    ~AnimationEventPolicy();
    AnimationEventPolicy(const AnimationEventPolicy&)=delete;
    AnimationEventPolicy& operator=(const AnimationEventPolicy&)=delete;
    // Called on the official engine frame before servicing owned instances.
    // Calls on other threads are transparent, with no managed reads.
    void ActivateOnCurrentThread();
    // Stop after owned instances finish retirement. Does not hot-remove hooks.
    void Stop() noexcept;
private:
    struct State;struct Holder;Holder* holder_=nullptr;
};
}
