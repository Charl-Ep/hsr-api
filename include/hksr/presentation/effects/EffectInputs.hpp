#pragma once
#include "../../metadata/RuntimeMetadata.hpp"
#include "../../managed/ManagedRoots.hpp"

namespace hksr::sdk_v1 {
struct EffectInputResult {CallResult target,completion;};
// Engine-thread reflection for the manager and two PUBLIC request references.
// FieldInfo.SetValue performs managed reference assignment; no presumed field
// offsets or manual GC pointer writes. The request must still be unsubmitted.
class HKSR_API EffectInputs {
public:
    EffectInputs(MethodResolver&,ObjectMetadata&,ManagedObjects&,ReferenceMethods);
    ~EffectInputs(); // no engine calls; explicit Close required
    EffectInputs(const EffectInputs&)=delete;
    EffectInputs& operator=(const EffectInputs&)=delete;
    ObjectResult Manager(); // unrooted result, read current global each time
    EffectInputResult Configure(void* retained_parameters,void* retained_target,void* retained_completion);
    bool Close();
private:struct State;State* state_;
};
}
