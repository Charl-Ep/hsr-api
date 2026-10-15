#pragma once
#include "EffectAccess.hpp"
#include "../../metadata/RuntimeMetadata.hpp"

namespace hksr::sdk_v1 {
struct EffectRelationsSnapshot {
    ObjectResult caster,target,param_entity,instance;
    Observed<std::uint32_t> id_before,id_after;
    Observed<bool> released;
    bool same_generation=false;
};
// Current request facts, NOT an ownership policy. The request's CasterEntity
// and TargetEntity have different meanings; null caster does not mean target.
// Read on the engine thread while the caller holds this request. No managed
// calls, allocations, hooks, or writes during Read; returned references are
// borrowed and must be retained before use across managed calls/frames.
// The caller supplies the current TriggerEffectParams declarations, not RVAs.
class HKSR_API EffectRelations {
public:
    explicit EffectRelations(const std::vector<DeclaredField>& request_fields);
    EffectRelationsSnapshot Read(EffectReference request) const;
private:
    std::size_t caster_,target_,param_entity_,instance_,id_,released_;
};
}
