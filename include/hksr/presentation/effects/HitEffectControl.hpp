#pragma once
#include "EffectControl.hpp"
#include "EffectMutex.hpp"
#include "../../world/WorldPlacement.hpp"

namespace hksr::sdk_v1 {
struct HitEffectPlacement {
    Position3 offset{},direction{};
    float delay=0;
    bool ignore_prefab_attach_point=false;
};
struct HitEffectControlMethods {
    Method align,refresh_attachment,replay;
    std::size_t request_offset=0,behavior_offset=0,effect_delay_offset=0;
    std::size_t position_offset=0,ignore_attach_offset=0,follow_first_frame_offset=0,delay_offset=0;
    bool fields_available=false;
    Method mutex_entity,mutex_global,first_active,first_fading,revive;
    std::size_t enable_replay_offset=0,request_replay_offset=0,native_pointer_offset=0;
    Method path_equal;
    std::size_t path_offset=0;
};
HKSR_API HitEffectControlMethods ResolveHitEffectControl(MethodResolver&,
    const std::vector<DeclaredField>& parameter_fields,const std::vector<DeclaredField>& effect_fields);
// Receipt-scoped damage-hit visual operations. The caller owns both the
// request and its cleanup. No manager search, modifier query or damage call.
class HKSR_API HitEffectControl {
public:
    HitEffectControl(EffectAccess,HitEffectControlMethods,ReferenceMethods);
    ~HitEffectControl();
    HitEffectControl(const HitEffectControl&)=delete;
    HitEffectControl& operator=(const HitEffectControl&)=delete;
    // Fresh private parameter block, before Submit. The acquisition owner is
    // responsible for target/caster references and completion registration.
    CallResult Configure(void* retained_parameters,const HitEffectPlacement&) const;
    EffectControlResult Loaded(EffectReference,void* completion_effect,const Position3& direction);
    EffectControlResult Update(void* retained_manager,EffectReference,const HitEffectPlacement&);
    EffectMutexDecision Mutex(void* manager,void* caster,void* target,void* path,const EffectMutexRule&,float delay,
        const std::vector<EffectReference>& replay_candidates);
    EffectControlResult Replay(EffectReference,EffectMutexKind);
    Observed<bool> Matches(EffectReference,void* caster,void* path);
    bool Close();
private:
    struct State;State* state_;
};
}
