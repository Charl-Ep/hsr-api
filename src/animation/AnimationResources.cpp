#include <hksr/animation/AnimationResources.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <array>
#include <stdexcept>

namespace hksr::sdk_v1 {
// Adapter owns names, including the version-specific emotion component.
ObjectResult ResolveAnimationComponentType(ManagedObjects&,AnimationComponent);
struct AnimationResources::State {
    EntityToken owner;
    EntityAccess entities;
    ManagedRoots roots;
    std::vector<ManagedRootId> persistent,temporary;
    std::array<void*,5> types{};
    unsigned calls=0;
    bool rooted=false,closing=false;
    State(EntityToken token,ReferenceMethods references,EntityAccess access)
        :owner(token),entities(std::move(access)),roots(references) {}
    struct Call {
        State& self;
        explicit Call(State& value):self(value){++self.calls;}
        ~Call(){--self.calls;}
    };
    void Active() const {
        if(closing)throw std::logic_error("animation resources closing");
        if(calls>1)throw std::logic_error("animation resources already in an operation");
        if(TrackedEntities().IsDisposed(owner) || TrackedEntities().Current(owner.entity).generation!=owner.generation)
            throw std::runtime_error("animation resource owner ended");
    }
    static std::size_t Index(AnimationComponent kind) {
        const auto index=static_cast<std::size_t>(kind);
        if(index>=5)throw std::invalid_argument("unknown animation component");
        return index;
    }
    void Keep(void* object,std::vector<ManagedRootId>& into) {
        if(!object)throw std::runtime_error("animation input is null");
        const auto id=roots.Keep(object);into.push_back(id);
        Active(); // Close/disposal may have happened inside retain/target.
        if(!roots.Status(id).ready)throw std::runtime_error("animation input root unavailable");
    }
    void Owner() {
        Active();
        if(!rooted){Keep(owner.entity,persistent);rooted=true;}
    }
    void Release(std::vector<ManagedRootId>& values) {
        for(auto it=values.begin();it!=values.end();) {
            if(roots.Release(*it).complete){roots.Forget(*it);it=values.erase(it);}
            else ++it; // ManagedRoots retains unknown outcomes without retry.
        }
    }
};
AnimationResources::AnimationResources(EntityToken owner,ReferenceMethods refs,EntityAccess entities)
    :state_(nullptr) {
    if(!owner.entity || !owner.generation)throw std::invalid_argument("animation resources require an entity epoch");
    state_=new State(owner,refs,std::move(entities));
}
AnimationResources::~AnimationResources(){delete state_;}
void AnimationResources::RetainOwner() {
    State::Call call(*state_);state_->Owner();
}
ObjectResult AnimationResources::Component(AnimationComponent kind,ManagedObjects& objects) {
    State::Call call(*state_);state_->Owner();
    auto& type=state_->types[State::Index(kind)];
    if(!type) {
        const auto found=ResolveAnimationComponentType(objects,kind);state_->Active();
        if(!found.call.returned || !found.object)throw std::runtime_error("animation component Type lookup failed");
        state_->Keep(found.object,state_->persistent);type=found.object;
    }
    const auto result=state_->entities.Component(state_->owner.entity,type);
    state_->Active();return result;
}
ObjectResult AnimationResources::CurrentComponent(AnimationComponent kind) {
    State::Call call(*state_);state_->Active();
    const auto type=state_->types[State::Index(kind)];
    if(!type)return {};
    const auto result=state_->entities.Component(state_->owner.entity,type);
    state_->Active();return result;
}
void* AnimationResources::Text(const std::string& value,ManagedObjects& objects) {
    State::Call call(*state_);state_->Owner();
    if(value.empty() || value.find('\0')!=std::string::npos)throw std::invalid_argument("invalid animation input text");
    const auto result=objects.NewStringUtf8(value.c_str());state_->Active();
    if(!result.call.returned || !result.object)throw std::runtime_error("animation input text allocation failed");
    state_->Keep(result.object,state_->temporary);return result.object;
}
void AnimationResources::Drain() {
    if(state_->calls)return;
    State::Call call(*state_);state_->Release(state_->temporary);
}
TextResult AnimationResources::StateName(void* animator,int hash,const AnimationControl& actions,const ComponentConfig& text) {
    State::Call call(*state_);state_->Owner();
    const auto name=actions.ResolveStateName(animator,hash);state_->Active();
    if(!name.call.returned || !name.object)return {{false,name.call.exception},{},"current Animator state name unavailable"};
    state_->Keep(name.object,state_->temporary);
    const auto roundtrip=actions.StateHash(name.object);state_->Active();
    if(!roundtrip.read.returned || roundtrip.value!=hash)return {{},{},"current Animator state name mismatch"};
    auto result=text.ReadText(name.object);state_->Active();
    if(result.utf8.empty() || result.utf8.find('\0')!=std::string::npos) {
        result.read.returned=false;result.error="current Animator state name empty or invalid";
    }
    return result;
}
bool AnimationResources::Close() {
    state_->closing=true;
    if(state_->calls)return false;
    State::Call call(*state_);
    state_->Release(state_->temporary);state_->Release(state_->persistent);
    return state_->temporary.empty() && state_->persistent.empty();
}
TextResult AnimationResources::LayerName(void* animator,int layer,const AnimationControl& actions,const ComponentConfig& text) {
    State::Call call(*state_);state_->Owner();
    const auto name=actions.LayerName(animator,layer);state_->Active();
    if(!name.call.returned || !name.object)return {{false,name.call.exception},{},"Animator layer name unavailable"};
    state_->Keep(name.object,state_->temporary);
    auto result=text.ReadText(name.object);state_->Active();
    if(result.utf8.empty() || result.utf8.find('\0')!=std::string::npos) {
        result.read.returned=false;result.error="Animator layer name empty or invalid";
    }
    return result;
}
}
