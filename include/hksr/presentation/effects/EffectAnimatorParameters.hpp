#pragma once
#include "EffectControl.hpp"
#include "../../managed/ManagedArrays.hpp"
#include <variant>

namespace hksr::sdk_v1 {
struct EffectParameterTrigger {};
using EffectParameterValue=std::variant<float,int,bool,EffectParameterTrigger>;
enum class EffectAnimatorSelection { FirstActive, FirstMatchingParameter };
struct EffectAnimatorParameter {
    std::string name;
    EffectParameterValue value=0.0f;
    EffectAnimatorSelection selection=EffectAnimatorSelection::FirstActive;
};
struct EffectParameterMethods {
    Method game_object,first_child,children,alive;
    Method parameters,parameter_name,parameter_type,string_equal;
    Method set_float,set_integer,set_bool,set_trigger;
    std::vector<std::string> binding_errors;
};
// Exact receipt, engine thread. Searches self and active children, applying
// once to the first Animator or first name-and-type match. Does not load an
// effect, broadcast, reset triggers, or evaluate battle DynamicFloat inputs.
class HKSR_API EffectAnimatorParameters {
public:
    EffectAnimatorParameters(EffectAccess,EffectParameterMethods,ReferenceMethods,ManagedObjects&,ManagedArrays,
        const std::vector<DeclaredField>& mono_effect_fields);
    ~EffectAnimatorParameters();
    EffectAnimatorParameters(const EffectAnimatorParameters&)=delete;
    EffectAnimatorParameters& operator=(const EffectAnimatorParameters&)=delete;
    EffectControlResult Set(EffectReference,const EffectAnimatorParameter&);
    bool Close();
private:
    struct State;State* state_;
};
HKSR_API EffectParameterMethods ResolveEffectAnimatorParameters460(MethodResolver&);
}
