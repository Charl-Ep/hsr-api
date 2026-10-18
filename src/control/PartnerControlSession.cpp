#include <hksr/control/PartnerControlSession.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <array>
#include <cstddef>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace hksr::sdk_v1 {
namespace {
struct AttachValue {
    void* self_point=nullptr;
    GameEntity* target=nullptr;
    void* target_point=nullptr;
    bool disable_when_invisible=false, use_target_scale=false;
    std::uint16_t padding=0;
    std::int32_t option=0;
    VisibilityFollow visibility=VisibilityFollow::None;
    std::uint16_t padding2=0;
    std::uint32_t padding3=0;
    GameEntity* extra_target=nullptr;
};
static_assert(sizeof(AttachValue)==48 && offsetof(AttachValue,visibility)==32 && offsetof(AttachValue,extra_target)==40);
struct VisibilityValue {
    void* key=nullptr;
    bool collider=false,trigger=false,audio=false;
    unsigned char padding[5]{};
};
static_assert(sizeof(VisibilityValue)==16 && offsetof(VisibilityValue,collider)==8 && offsetof(VisibilityValue,audio)==10);
}
struct PartnerControlSession::State {
    EntityOwner owner;EntityToken partner,summoner;GameWorld* world;
    PartnerControlMethods methods;ManagedObjects& objects;ManagedRoots roots;
    std::array<void*,5> types{};
    std::vector<ManagedRootId> cached,temporary;
    bool closed=false,busy=false;
    State(const EntityOwner& o,EntityToken p,EntityToken s,GameWorld* w,PartnerControlMethods m,ReferenceMethods r,ManagedObjects& obj)
        :owner(o),partner(p),summoner(s),world(w),methods(m),objects(obj),roots(r){}
    void Current(EntityToken token) const {
        const auto owned=owner.OwnedToken(token.entity);
        if(!token.entity || !token.generation || owned.generation!=token.generation || TrackedEntities().IsRetiring(token))
            throw std::runtime_error("partner relation entity ended or belongs to another client");
    }
    void Keep(void* value,std::vector<ManagedRootId>& into) {
        if(!value)throw std::runtime_error("partner relation reference absent");
        into.reserve(into.size()+1);into.push_back(roots.Keep(value));
        if(!roots.Status(into.back()).ready)throw std::runtime_error("partner relation reference retention failed");
    }
    bool Release(std::vector<ManagedRootId>& handles) {
        for(auto it=handles.begin();it!=handles.end();) {
            if(roots.Release(*it).complete){roots.Forget(*it);it=handles.erase(it);}else ++it;
        }
        return handles.empty();
    }
    void CheckWorld(EntityToken token) const {
        Current(token);GameWorld* found=nullptr;
        if(!native::Call(methods.owner_world,&found,token.entity).returned || found!=world)
            throw std::runtime_error("partner relation entity is not in the owner world");
        Current(token);
    }
    void CheckCurrent(EntityToken target) const {
        CheckWorld(partner);CheckWorld(summoner);if(target.entity)CheckWorld(target);
        Current(partner);Current(summoner);if(target.entity)Current(target);
    }
    void* Component(std::size_t index,const char* ns,const char* name) {
        if(!types[index]) {
            const auto result=objects.ManagedType(ns,name);
            if(!result.call.returned || !result.object)throw std::runtime_error(std::string(name)+" Type unavailable");
            Keep(result.object,cached);types[index]=result.object;
        }
        Current(partner);void* result=nullptr;
        if(!native::Call(methods.component,&result,partner.entity,types[index]).returned || !result)
            throw std::runtime_error(std::string(name)+" component unavailable");
        Keep(result,temporary);Current(partner);
        GameEntity* component_owner=nullptr;
        if(!native::CopyMemoryChecked(&component_owner,static_cast<unsigned char*>(result)+methods.component_owner,sizeof(component_owner)) || component_owner!=partner.entity)
            throw std::runtime_error("partner component owner differs");
        return result;
    }
    void* Text(const std::string& value) {
        if(value.find('\0')!=std::string::npos)throw std::invalid_argument("partner name must be UTF-8 without NUL");
        const auto text=objects.NewStringUtf8(value.c_str());
        if(!text.call.returned || !text.object)throw std::runtime_error("partner input string unavailable");
        Keep(text.object,temporary);return text.object;
    }
    PartnerControlResult Run(EntityToken target,const std::function<void(PartnerControlResult&)>& action) {
        PartnerControlResult result;
        if(closed || busy){result.error="partner relation session closed or reentered";return result;}
        busy=true;struct Guard{bool& flag;~Guard(){flag=false;}} guard{busy};
        try {
            result.calls.reserve(2);
            if(!world)throw std::invalid_argument("partner relation needs its world");
            CheckCurrent(target);
            Keep(partner.entity,temporary);Keep(summoner.entity,temporary);Keep(world,temporary);
            if(target.entity)Keep(target.entity,temporary);
            auto* group=Component(0,"","OLABFALNLJM");GameEntity* leader=nullptr;
            CheckCurrent(target);
            if(!native::Call(methods.group_leader,&leader,group).returned || leader!=summoner.entity || partner.entity==summoner.entity)
                throw std::runtime_error("partner is not a member of the supplied summoner");
            action(result);result.returned=true;
        }catch(const std::exception& e){result.error=e.what();}
        result.references_released=Release(temporary);return result;
    }
    template<class... Args>void Write(PartnerControlResult& result,EntityToken target,Method method,Args... args) {
        CheckCurrent(target);
        result.calls.push_back(native::CallVoid(method,args...));
        if(!result.calls.back().returned)throw std::runtime_error("partner relation call failed; mutation may be partial");
    }
};
PartnerControlSession::PartnerControlSession(const EntityOwner& o,EntityToken p,EntityToken s,GameWorld* w,
    PartnerControlMethods m,ReferenceMethods r,ManagedObjects& obj):state_(new State(o,p,s,w,m,r,obj)){}
PartnerControlSession::~PartnerControlSession(){delete state_;}
PartnerControlResult PartnerControlSession::SetMemberName(const std::string& name) {
    auto& s=*state_;const auto value=name;
    return s.Run({},[&](auto& out){auto* text=s.Text(value);auto* component=s.Component(0,"","OLABFALNLJM");s.Write(out,{},s.methods.member_name,component,text);});
}
PartnerControlResult PartnerControlSession::Attach(const PartnerAttachment& request) {
    auto& s=*state_;const auto input=request;
    return s.Run(input.target,[&](auto& out){
        if(!input.target.entity || input.target.entity==s.partner.entity)throw std::invalid_argument("attachment requires a distinct target");
        if(input.self_point.empty() || input.target_point.empty())throw std::invalid_argument("attachment requires both model attach-point names");
        AttachValue value;value.target=input.target.entity;
        value.self_point=s.Text(input.self_point);value.target_point=s.Text(input.target_point);
        value.disable_when_invisible=input.disable_when_invisible;value.use_target_scale=input.use_target_scale;value.visibility=input.visibility;
        auto* component=s.Component(1,"RPG.GameCore","TransformComponent");
        s.Write(out,input.target,s.methods.attach,component,&value);
        s.Write(out,input.target,s.methods.cancel_smooth_follow,component);
    });
}
PartnerControlResult PartnerControlSession::FollowAnimation(EntityToken target) {
    auto& s=*state_;
    return s.Run(target,[&](auto& out){
        if(target.entity==s.partner.entity)throw std::invalid_argument("animation follow target cannot be self");
        auto* component=s.Component(2,"","EAKGBLEMBLN");s.Write(out,target,s.methods.animation_follow,component,target.entity);
    });
}
PartnerControlResult PartnerControlSession::FollowVisibility(EntityToken target,VisibilityFollow follow) {
    auto& s=*state_;
    // The native setter treats None as removal even with a non-null target.
    if(follow==VisibilityFollow::None || !target.entity){target={};follow=VisibilityFollow::None;}
    return s.Run(target,[&](auto& out){
        if(target.entity==s.partner.entity)throw std::invalid_argument("visibility follow target cannot be self");
        auto* component=s.Component(3,"RPG.GameCore","CharacterVisibleComponent");
        s.Write(out,target,s.methods.visibility_follow,component,target.entity,follow);
    });
}
bool PartnerControlSession::Close() {
    auto& s=*state_;if(s.busy)return false;s.closed=true;s.types={};
    const bool a=s.Release(s.temporary),b=s.Release(s.cached);return a && b;
}
PartnerControlResult PartnerControlSession::SetVisibility(const PartnerVisibility& request) {
    auto& s=*state_;const auto input=request;
    return s.Run({},[&](auto& out){
        if(input.key.empty())throw std::invalid_argument("visibility requires a nonempty owner key");
        VisibilityValue value;value.key=s.Text(input.key);
        value.collider=input.mute_collider_when_hidden;value.trigger=input.mute_triggers_when_hidden;value.audio=input.mute_audio_when_hidden;
        auto* component=s.Component(3,"RPG.GameCore","CharacterVisibleComponent");
        s.Write(out,{},s.methods.visibility,component,input.visible,&value);
    });
}
PartnerControlResult PartnerControlSession::PlayIdle(const PartnerIdleAnimation& request) {
    auto& s=*state_;const auto input=request;
    return s.Run({},[&](auto& out){
        if(input.state.empty() || !std::isfinite(input.normalized_start) ||
            !std::isfinite(input.normalized_transition) || input.normalized_transition<0 ||
            !std::isfinite(input.normalized_end))throw std::invalid_argument("invalid partner idle animation input");
        auto* name=s.Text(input.state);
        auto* component=s.Component(4,"","CFFPEAALCPH");
        // TriggerAnimState's ordinary path passes start, transition and end
        // in this order. Idle(1) avoids the OnHit/Die gameplay state paths.
        // Extra, sequence request ID and the final request flag stay at the
        // factory's defaults; this API does not own an official task sequence.
        s.Write(out,{},s.methods.idle_animation,component,std::int32_t{1},name,
            input.normalized_start,input.normalized_transition,input.normalized_end,
            std::int32_t{0},std::uint32_t{0},false);
    });
}
}
