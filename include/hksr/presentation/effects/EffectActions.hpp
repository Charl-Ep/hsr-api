#pragma once
#include "EffectControl.hpp"
#include "EffectAnimatorParameters.hpp"
#include "../../managed/ManagedArrays.hpp"

namespace hksr::sdk_v1 {
struct EffectRemovalOptions {bool fade=true,immediate_fade=false,unbind=false;};
struct EffectRemovalResult {
    Observed<bool> removal;
    CallResult input_release;
    bool references_released=true;
    std::string detail;
};
struct EffectActionMethods {
    Method children,alive,set_trigger,reset_trigger,remove_input,remove,release_input;
    std::vector<std::string> binding_errors;
};
struct EffectActionServices {
    std::function<EffectControlResult(EffectReference,const std::string&,bool)> trigger;
    std::function<EffectRemovalResult(void*,EffectReference,const EffectRemovalOptions&)> remove;
    std::function<EffectControlResult(EffectReference,const EffectAnimatorParameter&)> parameter;
};
// Engine-thread operations on an exact effect receipt. Child enumeration
// includes inactive Animators. Removal delegates pending/active/streaming
// handling to the official manager, with an explicit nonzero request ID.
class HKSR_API EffectActions {
public:
    EffectActions(EffectAccess,EffectActionMethods,ReferenceMethods,ManagedObjects&,ManagedArrays,
        const std::vector<DeclaredField>& mono_effect_fields);
    ~EffectActions();
    EffectActions(const EffectActions&)=delete;
    EffectActions& operator=(const EffectActions&)=delete;
    EffectControlResult Trigger(EffectReference,const std::string& name,bool reset=false);
    EffectRemovalResult Remove(void* retained_manager,EffectReference,const EffectRemovalOptions&);
    bool Close();
private:
    struct State;State* state_;
};
HKSR_API EffectActionMethods ResolveEffectActions460(MethodResolver&);
}
