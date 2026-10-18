#pragma once
#include <hksr/presentation/PresentationPolicy.hpp>
#include <string>
#include <vector>

namespace hksr::sdk_v1::detail {
// Qualified shared-service task families, not character-specific exceptions.
// The construction policy applies these only after confirming entity ownership.
inline std::vector<std::string> DisabledOwnedAnimationTasks(AnimationTaskPolicy animation,
    PresentationIsolationPolicy isolation) {
    std::vector<std::string> result{
        "RPG.Client.AdventureGameplayLockWithDelay",
        "RPG.GameCore.GlobalTimeSlow",
        "RPG.GameCore.RemoveGlobalTimeSlow"};
    if(!animation.allow_camera_noise)result.emplace_back("RPG.GameCore.VCameraNoiseChange");
    if(!isolation.allow_timeline_environment)result.emplace_back("RPG.GameCore.GlobalMainIntensityEffect");
    return result;
}
}
