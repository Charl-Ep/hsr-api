#include <hksr/presentation/effects/EffectEntityFollow.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <limits>
#include <set>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct EffectEntityFollow::State {
    EffectAccess access;EffectEntityFollowMethods methods;EffectEntityFollowFields fields;
    ManagedObjects& objects;ManagedArrays arrays;ManagedReferenceList lists;ManagedRoots roots;
    EffectMotionTargetBinding target_binding;
    std::vector<ManagedRootId> temporary,types;
    void* plugin_type=nullptr;void* entity_type=nullptr;void* movement_type=nullptr;
    bool closed=false,busy=false;
    State(EffectAccess a,EffectEntityFollowMethods m,EffectEntityFollowFields f,ReferenceMethods refs,
        ManagedObjects& o,ManagedArrays ar,ManagedReferenceList l,EffectMotionTargetBinding target)
        :access(std::move(a)),methods(std::move(m)),fields(f),objects(o),arrays(std::move(ar)),lists(std::move(l)),roots(refs),target_binding(target){}
    void Keep(void* value,std::vector<ManagedRootId>& ids) {
        if(!value)throw std::runtime_error("follow reference absent");
        ids.reserve(ids.size()+1);ids.push_back(roots.Keep(value));
        if(!roots.Status(ids.back()).ready)throw std::runtime_error("follow reference retention failed");
    }
    bool Release(std::vector<ManagedRootId>& ids) {
        for(auto it=ids.begin();it!=ids.end();) {
            if(roots.Release(*it).complete){roots.Forget(*it);it=ids.erase(it);}else ++it;
        }
        return ids.empty();
    }
    void* Type(void*& cached,const char* ns,const char* name) {
        if(!cached) {
            const auto type=objects.ManagedType(ns,name);
            if(!type.call.returned || !type.object)throw std::runtime_error("follow component/entity type unavailable");
            Keep(type.object,types);cached=type.object;
        }
        return cached;
    }
    static void* Field(void* value,std::size_t offset) {
        void* result=nullptr;
        if(!value || !native::CopyMemoryChecked(&result,static_cast<unsigned char*>(value)+offset,sizeof(result)))
            throw std::runtime_error("follow relation unreadable");
        return result;
    }
    void Alive(void* value) {
        bool alive=false;
        if(!native::Call(methods.alive,&alive,value).returned || !alive)
            throw std::runtime_error("follow Unity object unavailable");
    }
    bool Current(EffectReference ref,void* effect,EffectControlResult& result) {
        const auto seen=access.Observe(ref);
        if(!seen.id_before.read.returned || !seen.id_after.read.returned || !seen.released.read.returned || !seen.instance.call.returned)
            throw std::runtime_error("follow request observation failed");
        if(!seen.same_generation || seen.released.value) {
            result.state=EffectControlState::Ended;result.detail="follow request released or reused";return false;
        }
        if(!seen.instance.object) {
            result.state=EffectControlState::NotLoaded;result.detail="follow effect still loading";return false;
        }
        if(seen.instance.object!=effect || Field(effect,fields.request)!=ref.parameters)
            throw std::runtime_error("follow effect request relation changed");
        return true;
    }
    std::vector<void*> Targets(void* behavior) {
        auto* list=Field(behavior,fields.targets);Keep(list,temporary);
        const auto count=lists.Count(list);
        if(!count.read.returned || count.value<0)throw std::runtime_error("follow target count unreadable");
        std::vector<void*> out;out.reserve(static_cast<std::size_t>(count.value));
        for(int i=0;i<count.value;++i) {
            const auto value=lists.Item(list,i);
            if(!value.call.returned)throw std::runtime_error("follow target unreadable");
            if(value.object)Keep(value.object,temporary);
            out.push_back(value.object);
        }
        return out;
    }
    EffectEntityFollowObservation Apply(EffectReference ref,const std::vector<void*>* expected,const std::vector<void*>* desired,
        const std::function<bool()>& still_current={}) {
        EffectEntityFollowObservation out;
        if(closed || busy || !ref.parameters){out.result.detail="follow service closed, reentered or request absent";return out;}
        // Copies survive callbacks that mutate the caller's program or input.
        const auto before=expected?*expected:std::vector<void*>{};
        const auto after=desired?*desired:std::vector<void*>{};
        const bool change=desired && before!=after;
        if(desired && (after.size()>static_cast<std::size_t>((std::numeric_limits<int>::max)()) ||
            std::set<void*>(after.begin(),after.end()).size()!=after.size() ||
            std::set<void*>(after.begin(),after.end()).count(nullptr))) {
            out.result.detail="follow targets must be nonnull distinct managed entities";return out;
        }
        busy=true;struct Exit{bool& flag;~Exit(){flag=false;}} exit{busy};
        try {
            if(!methods.component.code || !methods.alive.code || !methods.behavior.code || !methods.behavior_owner.code ||
                (change && !(after.empty()?methods.clear.code:methods.bind.code))) {
                out.result.state=EffectControlState::Unsupported;out.result.detail="follow operation binding unavailable";
            }else {
                Keep(ref.parameters,temporary);
                for(auto* target:before)if(target)Keep(target,temporary);
                for(auto* target:after)Keep(target,temporary);
                const auto seen=access.Observe(ref);auto* effect=seen.instance.object;
                if(seen.instance.call.returned && effect)Keep(effect,temporary);
                if(Current(ref,effect,out.result)) {
                    Alive(effect);void* plugin=nullptr;
                    auto* type=Type(plugin_type,"RPG.Client","MonoEffectPluginEntityFollowEffect");
                    if(!native::Call(methods.component,&plugin,effect,type).returned)
                        throw std::runtime_error("follow component lookup failed");
                    if(!plugin){out.result.state=EffectControlState::Unsupported;out.result.detail="follow plugin absent";}
                    else {
                        Keep(plugin,temporary);Alive(plugin);
                        if(Field(plugin,fields.plugin_owner)!=effect)throw std::runtime_error("follow plugin belongs to another effect");
                        void* behavior=nullptr;void* owner=nullptr;
                        if(!native::Call(methods.behavior,&behavior,plugin).returned || !behavior)
                            throw std::runtime_error("follow behavior unavailable");
                        Keep(behavior,temporary);
                        if(!native::Call(methods.behavior_owner,&owner,behavior).returned || owner!=plugin)
                            throw std::runtime_error("follow behavior owner differs");
                        out.targets=Targets(behavior);
                        if(expected && out.targets!=before)throw std::runtime_error("follow targets changed outside this binding");
                        void* input=nullptr;
                        if(change && !after.empty()) {
                            auto* element=Type(entity_type,"RPG.GameCore","GameEntity");
                            const auto array=arrays.Create(element,static_cast<int>(after.size()));
                            if(!array.call.returned || !array.object)throw std::runtime_error("follow target array allocation failed");
                            input=array.object;Keep(input,temporary);
                            for(std::size_t i=0;i<after.size();++i)
                                if(!arrays.Set(input,after[i],static_cast<int>(i)).returned)
                                    throw std::runtime_error("follow target array assignment failed");
                        }
                        if(Current(ref,effect,out.result)) {
                            if(!native::Call(methods.behavior_owner,&owner,behavior).returned || owner!=plugin ||
                                Field(plugin,fields.plugin_owner)!=effect || Field(behavior,fields.targets)==nullptr)
                                throw std::runtime_error("follow relation changed during input preparation");
                            if(expected && Targets(behavior)!=before)throw std::runtime_error("follow targets changed during input preparation");
                            if(still_current && !still_current())throw std::runtime_error("follow owner or target lifetime ended during input preparation");
                            if(change) {
                                out.result.call=after.empty()?native::CallVoid(methods.clear,behavior):native::CallVoid(methods.bind,behavior,input);
                                if(!out.result.call.returned)throw std::runtime_error("follow operation failed; binding may be partial");
                                out.targets=Targets(behavior);
                                if(out.targets!=after)throw std::runtime_error("follow operation returned without expected target list");
                            }
                            if(Current(ref,effect,out.result)) {
                                out.result.state=EffectControlState::Applied;
                                out.result.detail=change?"follow binding returned and target list matched":
                                    desired?"follow target list already matches":"follow target list observed";
                            }
                        }
                    }
                }
            }
        }catch(const std::exception& e){out.result.state=EffectControlState::Failed;out.result.detail=e.what();}
        out.result.references_released=Release(temporary);return out;
    }
};
EffectEntityFollow::EffectEntityFollow(EffectAccess a,EffectEntityFollowMethods m,EffectEntityFollowFields f,ReferenceMethods r,
    ManagedObjects& o,ManagedArrays ar,ManagedReferenceList l,EffectMotionTargetBinding target):state_(new State(std::move(a),std::move(m),f,r,o,std::move(ar),std::move(l),target)){}
EffectEntityFollow::~EffectEntityFollow(){delete state_;}
EffectEntityFollowObservation EffectEntityFollow::Read(EffectReference ref){return state_->Apply(ref,nullptr,nullptr);}
EffectMotionTargetObservation EffectEntityFollow::ReadTarget(void* entity) {
    auto& s=*state_;EffectMotionTargetObservation out;
    if(s.closed || s.busy || !entity){out.result.detail="effect motion target service closed, reentered or entity absent";return out;}
    if(!s.target_binding.component.code){out.result.state=EffectControlState::Unsupported;out.result.detail="effect motion target binding absent";return out;}
    s.busy=true;struct Exit{bool& flag;~Exit(){flag=false;}} exit{s.busy};
    try {
        s.Keep(entity,s.temporary);void* component=nullptr;
        auto* type=s.Type(s.movement_type,"","BGOBCKLPEMK");
        if(!native::Call(s.target_binding.component,&component,entity,type).returned)
            throw std::runtime_error("effect motion component lookup failed");
        if(!component){out.result.state=EffectControlState::Unsupported;out.result.detail="target has no effect movement component";}
        else {
            s.Keep(component,s.temporary);
            if(s.Field(component,s.target_binding.owner)!=entity)throw std::runtime_error("effect motion component owner changed");
            unsigned char attached=0;
            if(!native::CopyMemoryChecked(&attached,static_cast<unsigned char*>(component)+s.target_binding.attached,1))
                throw std::runtime_error("effect motion flag unreadable");
            out.attached=attached!=0;
            out.animation_motion=s.Field(component,s.target_binding.animation_motion)!=nullptr;
            out.other_motion=s.Field(component,s.target_binding.other_motion)!=nullptr;
            out.result.state=EffectControlState::Applied;
        }
    }catch(const std::exception& e){out.result.detail=e.what();}
    out.result.references_released=s.Release(s.temporary);return out;
}
EffectEntityFollowObservation EffectEntityFollow::Set(EffectReference ref,const std::vector<void*>& expected,const std::vector<void*>& targets,
    std::function<bool()> current){return state_->Apply(ref,&expected,&targets,current);}
bool EffectEntityFollow::Close(){auto& s=*state_;if(s.busy)return false;s.closed=true;s.plugin_type=nullptr;s.entity_type=nullptr;s.movement_type=nullptr;
    const bool a=s.Release(s.temporary),b=s.Release(s.types);return a && b;}
}
