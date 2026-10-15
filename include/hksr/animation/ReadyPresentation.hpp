#pragma once
#include "AnimationOverrides.hpp"
#include "OfficialCatalog.hpp"

namespace hksr::sdk_v1 {
struct ReadyPresentationResult {
    bool current=false;
    std::string detail;
};
// Scoped ready-entry operations from the official presentation path: reset the
// two configured hit layers and the unique configured default-expression layer.
// Does not install CFF, modify its caches, choose a ready pose or play the body.
// Optional missing layers report a detail, not a whole-action failure. current
// means the model and caller intent still match, not that every reset succeeded.
HKSR_API ReadyPresentationResult PrepareReadyPresentation(AnimationOverrideTarget target,
    void* animator,const ReadyAnimationDefaults& defaults,ManagedObjects& objects,const AnimationIntent& intent);
}
