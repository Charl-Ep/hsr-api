#pragma once
#include "PresentationSources.hpp"
#include "PresentationPolicy.hpp"
#include <functional>

namespace hksr::sdk_v1 {
class OwnedTimeline;
namespace detail {void ServiceRetiredPresentations() noexcept;}
// SDK boundaries, NOT an animation scheduler. Prepare before owned creation.
// Unknown sources retain original behavior with diagnostics, not a sandbox.
class PresentationIsolation {
public:
    using Trace=std::function<void(const std::string&)>;
    HKSR_API PresentationIsolation(RuntimeMetadata&,ReferenceMethods,PresentationIsolationPolicy,Trace);
    HKSR_API ~PresentationIsolation();
    PresentationIsolation(const PresentationIsolation&)=delete;
    PresentationIsolation& operator=(const PresentationIsolation&)=delete;
    HKSR_API void ActivateOnCurrentThread();
    HKSR_API void SetCameraPresentations(bool enabled);
    HKSR_API void RememberBirth(EntityOrigin);
    // Caller holds the entity alive. Empty for public or other-client sources.
    // Historical creation provenance, not permission to mutate a retired actor.
    HKSR_API EntityOrigin Source(GameEntity*) const;
    HKSR_API void RememberRequest(EffectReference,GameEntity*);
    HKSR_API void RetireRequest(EffectReference);
    HKSR_API std::unique_ptr<OwnedTimeline> CreateTimeline(EntityOrigin);
    // Shared service collection is coalesced per official runtime delivery.
    HKSR_API void Collect();
    // After client-owned requests/entities drain, detach ALL client callbacks
    // and transfer remaining official playback responsibility to the SDK tail.
    // No wait, stop/replay, or premature pool-return claim. Existing engine
    // frames collect weak observations/history. Not a hot-unload/total-cleanup
    // promise: the one-shot host, provider and hooks remain process resident.
    HKSR_API void Retire();
private:
    friend void detail::ServiceRetiredPresentations() noexcept;
    struct State;std::unique_ptr<State> state_;
};
}
