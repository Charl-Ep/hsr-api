#pragma once
#include "EffectQueries.hpp"
#include "EffectViewPolicy.hpp"

namespace hksr::sdk_v1::detail {
// Sources for per-effect plugin dispatch, visibility and FixedCRP name lookup.
// Scheduling, targets, asset configuration and cleanup remain original. The
// global manager's EndOfLateTick is NOT a single effect's source boundary.
class HKSR_API EffectScopes {
public:
    // Optional Framework policy for the audited MAIN-camera controller only.
    // false suppresses its writes, not effect-local CRP rendering. Empty keeps
    // the camera path entirely unchanged. Official/unknown sources are forwarded.
    using AllowMainCamera=std::function<bool(TimelineOwner)>;
    EffectScopes(MethodResolver&,ReferenceMethods,TimelineRegistryAccess::Fields,
        EffectQueries::Source,EffectQueries::Trace,AllowMainCamera={},
        std::shared_ptr<EffectViewPolicy> view={});
    ~EffectScopes();
    EffectScopes(const EffectScopes&)=delete;
    EffectScopes& operator=(const EffectScopes&)=delete;
    void ActivateOnCurrentThread();
    void Stop() noexcept;
private:
    struct State;struct Holder;Holder* holder_=nullptr;
};
}
