#pragma once
#include "../timeline/TimelineRegistryAccess.hpp"
#include <hksr/presentation/effects/EffectRelations.hpp>

namespace hksr::sdk_v1::detail {
// Query-only adapter. It does not create/update effects or replace their target.
// Known caller scopes use an independent list on a temporary manager receiver;
// the original FindEffectByUniqueName still performs all matching/selection.
// No scope is UNKNOWN, not public. Framework has not enabled this candidate.
class HKSR_API EffectQueries {
public:
    // Read-only source resolution, including retired entity provenance and any
    // explicitly owned request with a null CasterEntity. Never infer from target.
    // Empty = established public source. Throw for unknown; no game mutations.
    using Source=std::function<std::optional<TimelineOwner>(EffectReference,const EffectRelationsSnapshot&)>;
    using Trace=std::function<void(const std::string&)>;
    EffectQueries(MethodResolver&,ObjectMetadata&,ManagedObjects&,ReferenceMethods,
        TimelineRegistryAccess::Fields,Source,Trace);
    ~EffectQueries();
    EffectQueries(const EffectQueries&)=delete;
    EffectQueries& operator=(const EffectQueries&)=delete;
    void ActivateOnCurrentThread();
    void Stop() noexcept; // detach callbacks, no game calls or DLL unloading
private:
    struct State;struct Holder;Holder* holder_=nullptr;
};
}
