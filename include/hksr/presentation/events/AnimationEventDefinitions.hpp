#pragma once
#include "AnimationEventConstruction.hpp"
#include "../../managed/ManagedArrays.hpp"
#include "../../unity/ComponentConfig.hpp"

namespace hksr::sdk_v1 {
enum class AnimationEventPart { TimePoint, GroupEnter, GroupExit, Behavior };
// Describes a FINAL construction input, not an Animator transition or a task
// execution. Names describe membership; they are not proof of the active state.
struct AnimationEventDefinition {
    AnimationEventPart part=AnimationEventPart::TimePoint;
    int index=0;
    std::vector<std::string> states;
    std::string behavior;
    void* configurations=nullptr;
};
struct AnimationEventDefinitionIssue {AnimationEventPart part;int index;std::string error;};
struct AnimationEventDefinitions {
    std::vector<AnimationEventDefinition> rows;
    std::vector<AnimationEventDefinitionIssue> issues;
};
// Call inside AnimationEventConstruction::Before. Arrays and returned task
// collections remain borrowed from that construction scope. Does not load
// config files, construct tasks, execute events or pair arrays by index.
HKSR_API AnimationEventDefinitions ReadAnimationEventDefinitions(
    const AnimationEventSource&,ObjectMetadata&,ReferenceMethods,
    const ManagedArrays&,const ComponentConfig&);
}
