#pragma once
#include <hksr/unity/AssetLeases.hpp>
#include <functional>
#include <memory>
#include <string>

namespace hksr::sdk_v1 {
struct ActorEmotionMethods {
    Method graph, config_path, apply_config, select_mode;
    Method auto_blink, eye_blink, always_evaluate, built_graph;
};
HKSR_API ActorEmotionMethods ResolveActorEmotionMethods(MethodResolver&);

// Per-entity resource lifetime. Apply uses the same local operations as the
// official performance wrapper, without borrowing its world/asset slot.
class ActorEmotionSetup {
public:
    struct Status {
        bool attempted=false, ready=false, has_graph=false, requested_graph=false;
        bool config_loaded=false, graph_built=false, rolled_back=false;
        std::string error;
    };
    HKSR_API ActorEmotionSetup(ActorEmotionMethods,ReferenceMethods);
    HKSR_API void Apply(void* emotion,void* data,void* config_type,bool graph_mode);
    // Call only after the owning entity has actually been disposed.
    HKSR_API bool Close();
    const Status& State() const {return status_;}
private:
    ActorEmotionMethods methods_;
    AssetLeases assets_;
    AssetLeaseId config_=0;
    void* graph_=nullptr;
    Status status_;
};

// Replaces only the exploration phase's mode choice for explicitly owned
// owned independent entities. Every other component/thread calls the original method.
class ActorEmotionBoundary {
public:
    using Handler=std::function<bool(void*)>;
    HKSR_API ActorEmotionBoundary(Method phase_choice,Handler);
    HKSR_API ~ActorEmotionBoundary();
    HKSR_API void ActivateOnCurrentThread();
    HKSR_API void Stop() noexcept;
    ActorEmotionBoundary(const ActorEmotionBoundary&)=delete;
    ActorEmotionBoundary& operator=(const ActorEmotionBoundary&)=delete;
private:
    struct State;
    std::shared_ptr<State> state_;
};
}
