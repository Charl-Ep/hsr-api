#include <hksr/animation/AnimationOverrides.hpp>
#include <hksr/animation/ReadyPresentation.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <cmath>
#include <memory>
#include <limits>
#include <stdexcept>

namespace hksr::sdk_v1 {
namespace {
struct Access {
    AnimationOverrideTarget target;
    bool busy=false;
    explicit Access(AnimationOverrideTarget value):target(std::move(value)) {
        if(!target.owner.entity || !target.owner.generation)throw std::invalid_argument("animation override requires entity epoch");
    }
    bool Live(const AnimationIntent& intent) const {
        const auto owner=target.owner;
        return !TrackedEntities().IsDisposed(owner) && TrackedEntities().Current(owner.entity).generation==owner.generation &&
            (!intent || intent());
    }
    bool Model(void* animator,const AnimationIntent& intent,bool restoring=false) {
        if(!animator || !Live(intent))return false;
        const auto component=target.resources.CurrentComponent(AnimationComponent::Model);
        if(!Live(intent) || !component.call.returned || !component.object)return false;
        const auto model=target.models.Read(static_cast<CharacterModelComponent*>(component.object));
        return Live(intent) && model.changing.read.returned && (restoring || !model.changing.value) &&
            model.animator.read.returned && model.animator.value==animator;
    }
    bool Emotion(void* component,const AnimationIntent& intent) {
        if(!component || !Live(intent))return false;
        const auto now=target.resources.CurrentComponent(AnimationComponent::Emotion);
        return Live(intent) && now.call.returned && now.object==component;
    }
    struct Call {
        Access& self;
        explicit Call(Access& value):self(value) {
            if(self.busy)throw std::logic_error("animation override operation already active");
            self.busy=true;
        }
        ~Call(){self.busy=false;}
    };
    AnimatorStateObservation Effective(void* animator,int layer,const AnimationIntent& intent,bool restoring=false) {
        if(!Model(animator,intent,restoring))throw std::runtime_error("expression model/intent changed");
        const auto transition=target.actions.InTransition(animator,layer);
        if(!Model(animator,intent,restoring))throw std::runtime_error("expression model/intent changed");
        if(!transition.read.returned)throw std::runtime_error("expression transition unread");
        const auto result=transition.value?target.actions.NextState(animator,layer):target.actions.CurrentState(animator,layer);
        if(!Model(animator,intent,restoring))throw std::runtime_error("expression model/intent changed");
        if(!result.read.returned)throw std::runtime_error("expression effective state unread");
        return result;
    }
};
bool Matches(const AnimatorStateObservation& state,int hash) {
    return state.read.returned && (state.short_name_hash==hash || state.full_path_hash==hash);
}
AnimationOverrideResult Skipped(const char* reason){return {{},false,reason};}
AnimationOverrideResult Written(CallResult call,const char* success,const char* failure) {
    return {call,true,call.returned?success:failure};
}
}
struct ExpressionOverride::State:Access {
    void* animator;
    int hash,layer=-1;
    float blend;
    AnimatorStateObservation baseline;
    bool written=false,released=false;
    State(AnimationOverrideTarget target,void* value,int name,float duration)
        :Access(std::move(target)),animator(value),hash(name),blend(duration){}
};
ExpressionOverride::ExpressionOverride(AnimationOverrideTarget target,void* animator,int hash,float blend,
    const AnimationIntent& intent,const ExpressionOverride* predecessor):state_(nullptr) {
    if(!std::isfinite(blend) || blend<0)throw std::invalid_argument("invalid expression blend");
    auto state=std::make_unique<State>(std::move(target),animator,hash,blend);
    if(!state->Model(animator,intent))throw std::runtime_error("expression model/intent changed");
    const auto count=state->target.actions.LayerCount(animator);
    if(!count.read.returned || count.value<1)throw std::runtime_error("expression layer count unavailable");
    int matches=0;
    for(int layer=1;layer<count.value;++layer) {
        if(!state->Model(animator,intent))throw std::runtime_error("expression model/intent changed");
        const auto member=state->target.actions.HasState(animator,layer,hash);
        if(!member.read.returned)throw std::runtime_error("expression layer query failed");
        if(member.value){++matches;state->layer=layer;}
    }
    if(matches!=1)throw std::runtime_error("configured expression does not identify one current Animator layer");
    state->baseline=state->Effective(animator,state->layer,intent);
    if(!std::isfinite(state->baseline.normalized_time))throw std::runtime_error("expression baseline unavailable");
    if(predecessor) {
        const auto& previous=*predecessor->state_;
        if(previous.target.owner.entity==state->target.owner.entity && previous.target.owner.generation==state->target.owner.generation &&
            previous.animator==animator && previous.layer==state->layer && previous.written && !previous.released &&
            Matches(state->baseline,previous.hash))state->baseline=previous.baseline;
    }
    state_=state.release();
}
ExpressionOverride::~ExpressionOverride(){delete state_;}
AnimationOverrideResult ExpressionOverride::Apply(const AnimationIntent& intent) {
    auto& s=*state_;Access::Call call(s);
    if(s.released || s.written)return Skipped("expression lease already consumed");
    if(!s.Model(s.animator,intent))return Skipped("expression model/intent changed");
    s.written=true; // partial native failure still owns a possible write
    return Written(s.target.actions.CrossFade(s.animator,s.hash,s.blend,s.layer),
        "expression applied","expression transition failed");
}
AnimationOverrideResult ExpressionOverride::Release(const AnimationIntent& intent) {
    auto& s=*state_;Access::Call call(s);
    if(s.released)return Skipped("expression lease already released");
    s.released=true;
    if(!s.written || !s.Model(s.animator,intent,true))return Skipped("expression model changed or no owned write");
    const auto now=s.Effective(s.animator,s.layer,intent,true);
    if(!Matches(now,s.hash))return Skipped("expression successor retained");
    const int prior=s.baseline.full_path_hash?s.baseline.full_path_hash:s.baseline.short_name_hash;
    return Written(s.target.actions.CrossFade(s.animator,prior,s.blend,s.layer,s.baseline.normalized_time),
        "previous expression restored","expression restoration call failed");
}
struct BlinkOverride::State:Access {
    int source;
    void* component=nullptr;
    bool applied=false,written=false,released=false;
    State(AnimationOverrideTarget target,int value):Access(std::move(target)),source(value){}
};
BlinkOverride::BlinkOverride(AnimationOverrideTarget target,int source):state_(nullptr) {
    if(source<0)throw std::invalid_argument("negative blink source");
    state_=new State(std::move(target),source);
}
BlinkOverride::~BlinkOverride(){delete state_;}
AnimationOverrideResult BlinkOverride::Apply(ManagedObjects& objects,const AnimationIntent& intent) {
    auto& s=*state_;Access::Call call(s);
    if(s.applied || s.released)return Skipped("blink lease already consumed");
    s.applied=true;
    if(!s.Live(intent))return Skipped("blink owner/intent changed");
    const auto found=s.target.resources.Component(AnimationComponent::Emotion,objects);
    if(!s.Live(intent))return Skipped("blink owner/intent changed");
    if(!found.call.returned || !found.object)throw std::runtime_error("owned blink component unavailable");
    s.component=found.object;
    const auto source=s.target.actions.BlinkSource(s.component,s.source);
    if(!s.Emotion(s.component,intent))return Skipped("blink component changed; old source not touched");
    if(!source.active.read.returned || !source.enabled.read.returned)throw std::runtime_error("blink source snapshot unavailable");
    if(source.active.value && !source.enabled.value)return Skipped("existing disabled blink source retained; no ownership acquired");
    s.written=true;
    return Written(s.target.actions.SetBlinkSource(s.component,s.source,false),
        "official blink source disabled for configured expression","blink source call failed");
}
AnimationOverrideResult BlinkOverride::Release(const AnimationIntent& intent) {
    auto& s=*state_;Access::Call call(s);
    if(s.released)return Skipped("blink lease already released");
    s.released=true;
    if(!s.written || !s.Emotion(s.component,intent))return Skipped("blink component changed or no owned write");
    const auto source=s.target.actions.BlinkSource(s.component,s.source);
    if(!s.Emotion(s.component,intent))return Skipped("blink component changed; old source not touched");
    if(!source.active.read.returned || !source.enabled.read.returned)return Skipped("blink source restoration unread");
    if(!source.active.value || source.enabled.value)return Skipped("blink source already released by official owner");
    return Written(s.target.actions.SetBlinkSource(s.component,s.source,true),
        "own blink source released; other sources retained","blink source restore failed");
}
struct ReadySpeedOverride::State:Access {
    void* animator;
    int parameter=0;
    float baseline=0,written_value=0;
    bool acquired=false,written=false,released=false;
    State(AnimationOverrideTarget target,void* value):Access(std::move(target)),animator(value){}
};
ReadySpeedOverride::ReadySpeedOverride(AnimationOverrideTarget target,void* animator)
    :state_(new State(std::move(target),animator)){}
ReadySpeedOverride::~ReadySpeedOverride(){delete state_;}
bool ReadySpeedOverride::Owned()const{return state_->written && !state_->released;}
AnimationOverrideResult ReadySpeedOverride::Apply(float value,const AnimationIntent& intent) {
    auto& s=*state_;Access::Call call(s);
    if(!std::isfinite(value))throw std::invalid_argument("nonfinite ready speed");
    if(s.released || !s.Model(s.animator,intent))return Skipped("ready speed model/intent changed");
    if(!s.acquired) {
        const auto parameter=s.target.actions.ReadyTransitParameter();
        if(!s.Model(s.animator,intent))return Skipped("ready speed model/intent changed");
        if(!parameter.read.returned)return Skipped("ready speed parameter unavailable");
        const auto exists=s.target.actions.HasFloatParameter(s.animator,parameter.value);
        if(!s.Model(s.animator,intent))return Skipped("ready speed model/intent changed");
        if(!exists.read.returned || !exists.value)return Skipped("ready speed parameter absent/unread");
        const auto baseline=s.target.actions.FloatParameter(s.animator,parameter.value);
        if(!s.Model(s.animator,intent))return Skipped("ready speed model/intent changed");
        if(!baseline.read.returned || !std::isfinite(baseline.value))return Skipped("ready speed baseline unavailable");
        s.parameter=parameter.value;s.baseline=baseline.value;s.acquired=true;
    }
    s.written=true;s.written_value=value;
    return Written(s.target.actions.SetFloatParameter(s.animator,s.parameter,value),"ready speed applied","ready speed write failed");
}
AnimationOverrideResult ReadySpeedOverride::Release(const AnimationIntent& intent) {
    auto& s=*state_;Access::Call call(s);
    if(s.released)return Skipped("ready speed lease already released");
    s.released=true;
    if(!s.written || !s.Model(s.animator,intent,true))return Skipped("ready speed model changed or no owned write");
    const auto value=s.target.actions.FloatParameter(s.animator,s.parameter);
    if(!s.Model(s.animator,intent,true))return Skipped("ready speed model/intent changed");
    if(!value.read.returned || value.value!=s.written_value)return Skipped("ready speed changed externally or unread; not overwritten");
    return Written(s.target.actions.SetFloatParameter(s.animator,s.parameter,s.baseline),
        "ready speed restored","ready parameter restore failed");
}

ReadyPresentationResult PrepareReadyPresentation(AnimationOverrideTarget target,void* animator,
    const ReadyAnimationDefaults& defaults,ManagedObjects& objects,const AnimationIntent& intent) {
    Access access(std::move(target));
    auto& actions=access.target.actions;
    auto current=[&]{return access.Model(animator,intent);};
    ReadyPresentationResult result;
    auto issue=[&](const std::string& value){result.detail+="; ready presentation: "+value;};
    auto hash=[&](const std::string& value) {
        const auto text=access.target.resources.Text(value,objects);
        if(!current())throw std::runtime_error("ready presentation model/intent changed");
        const auto value_hash=actions.StateHash(text);
        if(!value_hash.read.returned)throw std::runtime_error("ready presentation state hash unavailable");
        return value_hash.value;
    };
    // Only configured hit layers, not every secondary Animator layer.
    for(const auto* name:{&defaults.hit_additive_layer,&defaults.hit_override_layer}) {
        if(!current())return result;
        if(!name->available || name->value.empty()){issue("hit layer name unavailable");continue;}
        void* text=nullptr;
        try {text=access.target.resources.Text(name->value,objects);}
        catch(const std::exception& error){if(!current())return result;issue(error.what());continue;}
        if(!current())return result;
        const auto index=actions.LayerIndex(animator,text);
        if(!current())return result;
        if(!index.read.returned){issue(name->value+" index unread");continue;}
        if(index.value<0){issue(name->value+" absent in current controller");continue;}
        if(index.value==0){issue(name->value+" unexpectedly resolves to body layer");continue;}
        const auto weight=actions.LayerWeight(animator,index.value);
        if(!current())return result;
        if(!weight.read.returned || !std::isfinite(weight.value)){issue(name->value+" weight unread");continue;}
        if(weight.value>0) {
            if(!defaults.empty_layer_state.available || defaults.empty_layer_state.value.empty()) {
                issue(name->value+" empty-state source unavailable");continue;
            }
            const int empty=hash(defaults.empty_layer_state.value);
            if(!current())return result;
            const auto member=actions.HasState(animator,index.value,empty);
            if(!current())return result;
            if(!member.read.returned || !member.value){issue(name->value+" empty state absent/unread");continue;}
            const auto evaluated=actions.Evaluate(animator,0);
            if(!current())return result;
            if(!evaluated.returned){issue(name->value+" zero-time evaluation failed");continue;}
            const auto cleared=actions.CrossFade(animator,empty,0,index.value,-std::numeric_limits<float>::infinity());
            if(!current())return result;
            if(!cleared.returned){issue(name->value+" empty transition failed");continue;}
        }
        const auto disabled=actions.SetLayerWeight(animator,index.value,0);
        if(!current())return result;
        if(!disabled.returned)issue(name->value+" weight reset failed");
        else issue(name->value+" layer="+std::to_string(index.value)+" old_weight="+std::to_string(weight.value)+
            (weight.value>0?" evaluated+emptied+disabled":" disabled without evaluation"));
    }
    if(!defaults.default_expression.available || defaults.default_expression.value.empty()) {
        issue("default expression unavailable");result.current=current();return result;
    }
    const int face=hash(defaults.default_expression.value);
    if(!current())return result;
    const auto count=actions.LayerCount(animator);
    if(!current())return result;
    if(!count.read.returned){issue("expression layers unread");result.current=true;return result;}
    int selected=-1,matches=0;
    for(int layer=1;layer<count.value;++layer) {
        const auto member=actions.HasState(animator,layer,face);
        if(!current())return result;
        if(!member.read.returned){issue("default expression membership unread");result.current=true;return result;}
        if(member.value){selected=layer;++matches;}
    }
    if(matches!=1){issue("default expression has "+std::to_string(matches)+" matching layers");result.current=true;return result;}
    // Official GKK(Default,-1) uses a 0.2 normalized blend. This is an entry
    // reset, not a temporary expression to restore when the ready pose ends.
    const auto applied=actions.CrossFade(animator,face,0.2f,selected);
    if(!current())return result;
    if(!applied.returned)issue("default expression transition failed");
    else issue("default expression="+defaults.default_expression.value+" layer="+std::to_string(selected)+" blend=0.2 normalized");
    result.current=true;return result;
}
}
