#pragma once
#include "AnimationControl.hpp"
#include "AnimationResources.hpp"
#include <functional>

namespace hksr::sdk_v1 {
// Borrowed resources must outlive these leases. Resolve their Model component
// before constructing Animator overrides. Operations run on the owner's
// official thread. The synchronous predicate is caller intent, not an engine
// lifetime check; the SDK additionally checks entity/model/component identity.
// It is never retained. Destructors discard native records, never call Unity.
struct AnimationOverrideTarget {
    EntityToken owner;
    AnimationResources& resources;
    const ModelAccess& models;
    const AnimationControl& actions;
};
using AnimationIntent = std::function<bool()>;
struct AnimationOverrideResult {
    CallResult call;
    bool attempted=false;
    std::string detail;
};

// Capture before replacing an existing expression, then Apply after the caller
// has submitted its body action. A predecessor can pass on the original
// baseline only while its own expression still occupies this same layer.
class HKSR_API ExpressionOverride {
public:
    ExpressionOverride(AnimationOverrideTarget target,void* animator,int hash,float blend,
        const AnimationIntent& intent,const ExpressionOverride* predecessor=nullptr);
    ~ExpressionOverride();
    ExpressionOverride(const ExpressionOverride&)=delete;
    ExpressionOverride& operator=(const ExpressionOverride&)=delete;
    AnimationOverrideResult Apply(const AnimationIntent& intent);
    AnimationOverrideResult Release(const AnimationIntent& intent);
    // Replace the owned expression with the caller's freshly resolved return
    // state. A later writer or a different model is never overwritten.
    AnimationOverrideResult ReleaseTo(int hash,float blend,const AnimationIntent& intent);
private:
    struct State;
    State* state_;
};

// Disables only the supplied official source, not the entire blink mask.
// A pre-existing disabled source is observed but never claimed or restored.
class HKSR_API BlinkOverride {
public:
    BlinkOverride(AnimationOverrideTarget target,int source);
    ~BlinkOverride();
    BlinkOverride(const BlinkOverride&)=delete;
    BlinkOverride& operator=(const BlinkOverride&)=delete;
    AnimationOverrideResult Apply(ManagedObjects& objects,const AnimationIntent& intent);
    AnimationOverrideResult Release(const AnimationIntent& intent);
private:
    struct State;
    State* state_;
};

// Uses the official ready-transition float parameter, never Animator.speed.
// Absence leaves the independent ready pose usable; this lease does not choose
// transition windows, direction, fallback poses, or playback completion policy.
class HKSR_API ReadySpeedOverride {
public:
    ReadySpeedOverride(AnimationOverrideTarget target,void* animator);
    ~ReadySpeedOverride();
    ReadySpeedOverride(const ReadySpeedOverride&)=delete;
    ReadySpeedOverride& operator=(const ReadySpeedOverride&)=delete;
    AnimationOverrideResult Apply(float value,const AnimationIntent& intent);
    AnimationOverrideResult Release(const AnimationIntent& intent);
    bool Owned() const;
private:
    struct State;
    State* state_;
};
}
