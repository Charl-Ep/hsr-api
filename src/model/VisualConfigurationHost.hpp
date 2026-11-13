#pragma once
#include <hksr/model/ActorVisualConfiguration.hpp>
#include <hksr/entity/EntityAccess.hpp>
#include <hksr/presentation/events/AnimationEvents.hpp>

namespace hksr::api::detail {
// Captures exact owned components for configuration bindings. The context only
// composes this host; managed type roots and model identity belong to it.
class VisualConfigurationHost {
  public:
    VisualConfigurationHost(MethodResolver&, ObjectMetadata&, ManagedObjects&, ActorVisualConfigurationFactory::Fields,
                            ReferenceMethods, ManagedArrays, EntityAccess, std::function<bool(EntityToken)>,
                            AnimationEvents);
    ~VisualConfigurationHost();
    std::unique_ptr<ActorVisualConfiguration> Create(EntityToken, GameWorld*);
    bool Close();

  private:
    struct State;
    std::shared_ptr<State> state_;
};
}
