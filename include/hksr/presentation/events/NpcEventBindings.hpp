#pragma once
#include "EventRegistrationLease.hpp"
#include "../../animation/AdventureConfig.hpp"
#include "../../entity/EntityAccess.hpp"

namespace hksr::sdk_v1 {
// Explicit low-level composition for synthetic or custom providers.
struct NpcEventBindings {
    EntityAccess entities;
    AdventureDataField data_field;
    AdventureConfig config;
    AnimationEvents events;
    ManagedArrays arrays;
    ManagedReferenceList lists;
    SceneObjects strings;
};
}
