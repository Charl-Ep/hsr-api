#pragma once
#include "ActorControl.hpp"
#include "../unity/OwnedUnityObjects.hpp"
#include "../managed/ManagedArrays.hpp"

namespace hksr::sdk_v1 {
// Explicit low-level composition for adapters and test providers.
// Ordinary clients obtain sessions from PresentationContext::Controls().
struct ActorControlBindings {
    ReferenceMethods references;
    UnityLifetimeMethods lifetime;
    EntityAccessMethods entity;
    ActorControlMethods control;
    WorldPlacementMethods placement;
    ArrayMethods arrays;
};
}
