#pragma once
#include <memory>
#include "../../managed/References.hpp"
#include <string>
#include <vector>

namespace hksr::sdk_v1 {
// Qualified x64 unboxed TriggerEffectParamsRef layout; not a managed object.
struct EffectReference {std::uint32_t id=0,padding=0;void* parameters=nullptr;};
static_assert(sizeof(EffectReference)==16);
struct EffectAcquireResult {EffectReference reference;CallResult call;};
struct EffectObservation {
    Observed<std::uint32_t> id_before,id_after;
    Observed<bool> released;
    Observed<int> state;
    ObjectResult instance;
    bool same_generation=false;
};
struct EffectAccessMethods {
    Method map_path,acquire,submit,remove,release,id,released,state,instance;
    std::vector<std::string> binding_errors;
    Method path_empty;
};
// Thin engine-thread calls. Inputs/results must be retained by the request
// owner. This API does not configure a request, attach callbacks, poll itself,
// or infer ownership from successful void returns.
class EffectAccess {
public:
    HKSR_API explicit EffectAccess(EffectAccessMethods methods);
    HKSR_API ~EffectAccess();
    HKSR_API EffectAccess(const EffectAccess&) noexcept;
    HKSR_API EffectAccess& operator=(const EffectAccess&) noexcept;
    HKSR_API ObjectResult MapPath(void* entity,void* rooted_path) const;
    HKSR_API EffectAcquireResult Acquire(void* rooted_mapped_path) const;
    HKSR_API EffectObservation Observe(EffectReference reference) const;
    HKSR_API CallResult Submit(void* manager,EffectReference reference) const;
    // Official integrated removal covers active, pending and streaming effects.
    // true means matched/submitted removal, NOT synchronous destruction/pooling.
    // A zero ID is not forwarded: the official filter treats it as a wildcard.
    HKSR_API Observed<bool> Remove(void* manager,EffectReference reference,bool fade) const;
    // ONLY for an acquired request which was never passed to Submit, or whose
    // failure ownership has separately been established. Never after Remove.
    HKSR_API CallResult ReleaseUnsubmitted(EffectReference reference) const;
private:std::shared_ptr<const EffectAccessMethods> methods_;
};
HKSR_API EffectAccessMethods ResolveEffectAccess460(MethodResolver& metadata);
}
