#pragma once
#include "EffectControl.hpp"
#include "../../managed/ManagedArrays.hpp"
#include "../../managed/ManagedLists.hpp"

namespace hksr::sdk_v1 {
struct EffectEntityFollowMethods {
    Method component,alive,behavior,behavior_owner,bind,clear;
    std::vector<std::string> binding_errors;
};
struct EffectEntityFollowFields {
    std::size_t request=0,plugin_owner=0,targets=0;
};
struct EffectMotionTargetBinding {Method component;std::size_t owner=0,attached=0;};
struct EffectMotionTargetObservation {EffectControlResult result;bool attached=false;};
struct EffectEntityFollowObservation {
    EffectControlResult result;
    // Borrowed identities, not ownership or entity-lifetime receipts.
    std::vector<void*> targets;
};
// Engine-thread access to one request's MonoEffectPluginEntityFollowEffect.
// Bind copies the supplied entities into the official plugin and enables its
// normal OnAnimatorMove processing. Clear undoes its accumulated translation.
// Neither operation ticks animation, forces loading, or searches by name.
//
// The caller owns the request AND target lifetimes and must pair Bind/Clear
// before retiring targets. This adapter does not arbitrate multiple motion
// owners. expected_targets makes an ownership change observable rather than
// clearing an unrelated binding. Empty desired_targets uses official Clear.
class HKSR_API EffectEntityFollow {
public:
    EffectEntityFollow(EffectAccess,EffectEntityFollowMethods,EffectEntityFollowFields,
        ReferenceMethods,ManagedObjects&,ManagedArrays,ManagedReferenceList,EffectMotionTargetBinding={});
    ~EffectEntityFollow();
    EffectEntityFollow(const EffectEntityFollow&)=delete;
    EffectEntityFollow& operator=(const EffectEntityFollow&)=delete;
    EffectEntityFollowObservation Read(EffectReference);
    // Explicit live target only. Reads its existing official movement flag;
    // does not create a movement component or infer ownership from the flag.
    EffectMotionTargetObservation ReadTarget(void* entity);
    EffectEntityFollowObservation Set(EffectReference,const std::vector<void*>& expected_targets,
        const std::vector<void*>& desired_targets,std::function<bool()> still_current={});
    bool Close();
private:
    struct State;State* state_;
};
HKSR_API EffectEntityFollowMethods ResolveEffectEntityFollow460(MethodResolver&);
HKSR_API EffectMotionTargetBinding ResolveEffectMotionTarget460(MethodResolver&,const std::vector<DeclaredField>&);
HKSR_API EffectEntityFollowFields ResolveEffectEntityFollowFields460(
    const std::vector<DeclaredField>& effect,const std::vector<DeclaredField>& plugin,
    const std::vector<DeclaredField>& behavior);
}
