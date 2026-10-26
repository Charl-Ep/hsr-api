#include <hksr/presentation/effects/HitEffectControl.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <cmath>
#include <stdexcept>

namespace hksr::sdk_v1 {
namespace {
bool Finite(Position3 p){return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);}
template<class T>bool Write(void* object,std::size_t offset,const T& value){
    return object && native::CopyMemoryChecked(static_cast<unsigned char*>(object)+offset,&value,sizeof(value));
}
std::size_t Field(const std::vector<DeclaredField>& fields,const char* owner,const char* name,const char* type) {
    const DeclaredField* found=nullptr;
    for(const auto& f:fields)if(f.owner_namespace=="RPG.Client" && f.owner_name==owner && f.name==name) {
        if(found || f.type!=type || !f.flags_observed || (f.flags&0x10))throw std::runtime_error(std::string("hit effect field differs: ")+name);
        found=&f;
    }
    if(!found)throw std::runtime_error(std::string("hit effect field absent: ")+name);
    return found->offset;
}
bool Position(void* parameters,const HitEffectControlMethods& m,const HitEffectPlacement& p) {
    struct NullableVector {Position3 value;std::uint8_t present=1;std::uint8_t padding[3]{};};
    static_assert(sizeof(NullableVector)==16);
    return Write(parameters,m.position_offset,NullableVector{p.offset}) &&
        Write(parameters,m.ignore_attach_offset,static_cast<std::uint8_t>(p.ignore_prefab_attach_point));
}
}
HitEffectControlMethods ResolveHitEffectControl(MethodResolver& resolver,
    const std::vector<DeclaredField>& parameters,const std::vector<DeclaredField>& effects) {
    HitEffectControlMethods out;
    out.align=resolver.Resolve("","PGOOHIHKHNJ","DMFKGEBBGGG","System.Void",
        {"RPG.Client.MonoEffect","RPG.MVector3"},MethodKind::Static);
    out.refresh_attachment=resolver.Resolve("RPG.Client","MonoEffectManager","_TryGetAttachPointFromEntityAfterLoadEffect","System.Void",
        {"RPG.Client.TriggerEffectParams","RPG.Client.MonoEffect"});
    out.replay=resolver.Resolve("RPG.Client","MonoEffectBehavior","Replay","System.Void",{"System.Boolean"});
    out.revive=resolver.Resolve("RPG.Client","MonoEffectBehavior","Revive","System.Void",{});
    out.mutex_entity=resolver.Resolve("RPG.GameCore","GamePlayStatic","IsTriggerEffectMutexEntity","RPG.Client.EffectMutexResult",
        {"RPG.Client.MonoEffectManager","System.String","System.Single","System.Int32","RPG.GameCore.GameEntity","System.Single"},MethodKind::Static);
    out.mutex_global=resolver.Resolve("RPG.GameCore","GamePlayStatic","IsTriggerEffectMutexGlobal","RPG.Client.EffectMutexResult",
        {"RPG.Client.MonoEffectManager","System.String","System.Single","System.Int32"},MethodKind::Static);
    out.first_active=resolver.Resolve("RPG.Client","MonoEffectManager","GetFirstActiveEffect","RPG.Client.MonoEffect",
        {"System.String","RPG.GameCore.GameEntity"});
    out.first_fading=resolver.Resolve("RPG.Client","MonoEffectManager","GetFirstFadeOutEffect","RPG.Client.MonoEffect",
        {"System.String","RPG.GameCore.GameEntity"});
    out.request_offset=Field(effects,"MonoEffect","TriggerParams","RPG.Client.TriggerEffectParams");
    out.behavior_offset=Field(effects,"MonoEffect","_Behavior","RPG.Client.MonoEffectBehavior");
    out.effect_delay_offset=Field(effects,"MonoEffect","Delay","System.Single");
    out.position_offset=Field(parameters,"TriggerEffectParams","PositionOffset","System.Nullable<UnityEngine.Vector3>");
    out.ignore_attach_offset=Field(parameters,"TriggerEffectParams","IgnorePrefabAttachPoint","System.Boolean");
    out.follow_first_frame_offset=Field(parameters,"TriggerEffectParams","FollowFirstFrameOnLateTick","System.Boolean");
    out.delay_offset=Field(parameters,"TriggerEffectParams","Delay","System.Single");
    out.enable_replay_offset=Field(effects,"MonoEffect","EnableReplay","System.Boolean");
    out.request_replay_offset=Field(parameters,"TriggerEffectParams","IsEnableReplay","System.Boolean");
    out.path_equal=resolver.Resolve("System","String","op_Equality","System.Boolean",{"System.String","System.String"},MethodKind::Static);
    out.path_offset=Field(effects,"MonoEffect","<EffectName>k__BackingField","System.String");
    // Unity's inherited native-object field retains its actual declaring type.
    for(const auto& f:effects)if(f.owner_namespace=="UnityEngine" && f.owner_name=="Object" && f.name=="m_CachedPtr" &&
        f.type=="System.IntPtr" && f.flags_observed && !(f.flags&0x10))out.native_pointer_offset=f.offset;
    if(!out.native_pointer_offset)throw std::runtime_error("hit effect native identity field absent");
    out.fields_available=true;return out;
}
struct HitEffectControl::State {
    EffectAccess access;HitEffectControlMethods methods;ManagedRoots roots;
    std::vector<ManagedRootId> retained;
    bool closed=false,busy=false;
    State(EffectAccess a,HitEffectControlMethods m,ReferenceMethods r):access(std::move(a)),methods(std::move(m)),roots(r){}
    void Keep(void* object) {
        retained.reserve(retained.size()+1);const auto id=roots.Keep(object);retained.push_back(id);
        if(!roots.Status(id).ready)throw std::runtime_error("hit effect retention failed");
    }
    bool Release() {
        for(auto i=retained.begin();i!=retained.end();) {
            if(roots.Release(*i).complete){roots.Forget(*i);i=retained.erase(i);}else ++i;
        }
        return retained.empty();
    }
    bool Current(EffectReference ref,void* expected,EffectControlResult& out,bool completion=false) {
        const auto seen=access.Observe(ref);
        if(!seen.id_before.read.returned || !seen.id_after.read.returned || !seen.released.read.returned || !seen.instance.call.returned) {
            out.detail="hit effect observation failed";return false;
        }
        if(!seen.same_generation || seen.released.value){out.state=EffectControlState::Ended;out.detail="hit effect released or reused";return false;}
        if(!seen.instance.object && !(completion && expected)){out.state=EffectControlState::NotLoaded;out.detail="hit effect is loading";return false;}
        void* owner=nullptr;
        if((seen.instance.object && seen.instance.object!=expected) || !native::CopyMemoryChecked(&owner,static_cast<unsigned char*>(expected)+methods.request_offset,sizeof(owner)) || owner!=ref.parameters) {
            out.detail="hit effect request relation changed";return false;
        }
        return true;
    }
    EffectControlResult Apply(void* manager,EffectReference ref,void* completion,const HitEffectPlacement& p,bool update) {
        EffectControlResult out;
        if(closed || busy || !ref.parameters){out.detail="hit effect controller unavailable";return out;}
        if(!Finite(p.direction) || (update && (!Finite(p.offset) || !std::isfinite(p.delay)))) {
            out.detail="invalid hit effect placement";return out;
        }
        if(!methods.fields_available || !methods.align.code || (update && (!manager || !methods.refresh_attachment.code || !methods.replay.code))) {
            out.state=EffectControlState::Unsupported;out.detail="hit effect binding unavailable";return out;
        }
        busy=true;struct Exit{bool& value;~Exit(){value=false;}} exit{busy};
        try {
            Keep(ref.parameters);if(manager)Keep(manager);
            auto* effect=completion;
            if(update) {
                const auto seen=access.Observe(ref);
                if(seen.instance.call.returned)effect=seen.instance.object;
            }
            if(effect)Keep(effect);
            if(Current(ref,effect,out,!update)) {
                bool ready=true;
                if(update) {
                    void* behavior=nullptr;
                    if(!native::CopyMemoryChecked(&behavior,static_cast<unsigned char*>(effect)+methods.behavior_offset,sizeof(behavior)) || !behavior)
                        throw std::runtime_error("hit effect behavior absent");
                    Keep(behavior);
                    if(!Current(ref,effect,out))ready=false;
                    else if(!Position(ref.parameters,methods,p) || !Write(effect,methods.effect_delay_offset,p.delay))
                        throw std::runtime_error("hit effect update write failed");
                    else {
                        out.call=native::CallVoid(methods.refresh_attachment,manager,ref.parameters,effect);
                        ready=out.call.returned && Current(ref,effect,out);
                        if(ready) {
                            out.call=native::CallVoid(methods.replay,behavior,false);
                            ready=out.call.returned && Current(ref,effect,out);
                        }
                    }
                }
                if(ready) {
                    out.call=native::CallVoid(methods.align,effect,p.direction);
                    out.state=out.call.returned?EffectControlState::Applied:EffectControlState::Failed;
                    out.detail=out.call.returned?"hit effect visual update returned":"hit effect visual update failed";
                }
            }
        }catch(const std::exception& e){out.state=EffectControlState::Failed;out.detail=e.what();}
        out.references_released=Release();return out;
    }
};
HitEffectControl::HitEffectControl(EffectAccess a,HitEffectControlMethods m,ReferenceMethods r):state_(new State(std::move(a),std::move(m),r)){}
HitEffectControl::~HitEffectControl(){delete state_;}
CallResult HitEffectControl::Configure(void* parameters,const HitEffectPlacement& p) const {
    CallResult out;const auto& s=*state_;
    if(s.closed || !s.methods.fields_available || !parameters || !Finite(p.offset) || !Finite(p.direction) || !std::isfinite(p.delay))return out;
    out.returned=Position(parameters,s.methods,p) && Write(parameters,s.methods.follow_first_frame_offset,std::uint8_t{0}) &&
        Write(parameters,s.methods.delay_offset,p.delay);
    return out;
}
EffectControlResult HitEffectControl::Loaded(EffectReference ref,void* effect,const Position3& direction){
    HitEffectPlacement p;p.direction=direction;return state_->Apply(nullptr,ref,effect,p,false);
}
EffectControlResult HitEffectControl::Update(void* manager,EffectReference ref,const HitEffectPlacement& p){return state_->Apply(manager,ref,nullptr,p,true);}
EffectMutexDecision HitEffectControl::Mutex(void* manager,void* caster,void* target,void* path,const EffectMutexRule& rule,float delay,
    const std::vector<EffectReference>& candidates) {
    EffectMutexDecision out;auto& s=*state_;
    if(s.closed || s.busy || !manager || !path || !std::isfinite(rule.interval) || !std::isfinite(delay)) {
        out.error="effect mutex input unavailable";return out;
    }
    s.busy=true;struct Exit{bool& value;~Exit(){value=false;}} exit{s.busy};
    try {
        s.Keep(manager);s.Keep(path);if(target)s.Keep(target);
        if(caster) {
            s.Keep(caster);const auto mapped=s.access.MapPath(caster,path);
            if(!mapped.call.returned || !mapped.object)throw std::runtime_error("hit effect path mapping failed");
            s.Keep(mapped.object);path=mapped.object;
        }
        if(rule.scope==EffectMutexScope::Entity) {
            if(!target)throw std::runtime_error("entity mutex requires target");
            out.kind.read=native::Call(s.methods.mutex_entity,&out.kind.value,manager,path,rule.interval,rule.maximum,target,delay);
        }else if(rule.scope==EffectMutexScope::Global)
            out.kind.read=native::Call(s.methods.mutex_global,&out.kind.value,manager,path,rule.interval,rule.maximum);
        else {out.kind.read.returned=true;out.kind.value=EffectMutexKind::None;}
        if(!out.kind.read.returned)throw std::runtime_error("official effect mutex query failed");
        if(out.kind.value!=EffectMutexKind::None && out.kind.value!=EffectMutexKind::Default && out.kind.value!=EffectMutexKind::FadeOut)
            throw std::runtime_error("official effect mutex result differs");
        if(out.kind.value!=EffectMutexKind::None && !candidates.empty()) {
            const auto method=out.kind.value==EffectMutexKind::Default?s.methods.first_active:s.methods.first_fading;
            void* effect=nullptr;
            if(!native::Call(method,&effect,manager,path,rule.scope==EffectMutexScope::Entity?target:nullptr).returned)
                throw std::runtime_error("effect mutex candidate query failed");
            if(effect) {
                s.Keep(effect);
                for(const auto ref:candidates) {
                    s.Keep(ref.parameters);EffectControlResult match;
                    if(s.Current(ref,effect,match)){out.replay=ref;break;}
                }
            }
        }
    }catch(const std::exception& e){out.error=e.what();}
    if(!s.Release())out.error+="; effect mutex references pending release";
    return out;
}
EffectControlResult HitEffectControl::Replay(EffectReference ref,EffectMutexKind kind) {
    EffectControlResult out;auto& s=*state_;
    if(s.closed || s.busy || !ref.parameters || kind==EffectMutexKind::None){out.detail="effect replay unavailable";return out;}
    s.busy=true;struct Exit{bool& value;~Exit(){value=false;}} exit{s.busy};
    try {
        s.Keep(ref.parameters);
        const auto observation=s.access.Observe(ref);auto* effect=observation.instance.object;
        if(effect)s.Keep(effect);
        if(s.Current(ref,effect,out)) {
            void* native_object=nullptr;void* behavior=nullptr;std::uint8_t enabled=0,parameter_enabled=0;
            const auto read=[&](void* p,std::size_t offset,auto& value){
                if(!native::CopyMemoryChecked(&value,static_cast<unsigned char*>(p)+offset,sizeof(value)))throw std::runtime_error("effect replay input unread");
            };
            read(effect,s.methods.native_pointer_offset,native_object);
            read(effect,s.methods.enable_replay_offset,enabled);read(ref.parameters,s.methods.request_replay_offset,parameter_enabled);
            if(!native_object || (kind==EffectMutexKind::Default && !enabled && !parameter_enabled)) {
                out.state=EffectControlState::NoMatch;out.detail="effect does not accept mutex replay";
            }else {
                read(effect,s.methods.behavior_offset,behavior);s.Keep(behavior);
                if(s.Current(ref,effect,out)) {
                    out.call=native::CallVoid(s.methods.revive,behavior);
                    if(out.call.returned && s.Current(ref,effect,out)) {
                        out.call=native::CallVoid(s.methods.replay,behavior,false);
                        if(out.call.returned && s.Current(ref,effect,out))out.state=EffectControlState::Applied;
                    }
                }
            }
        }
    }catch(const std::exception& e){out.detail=e.what();}
    out.references_released=s.Release();return out;
}
bool HitEffectControl::Close(){auto& s=*state_;if(s.busy)return false;s.closed=true;return s.Release();}
Observed<bool> HitEffectControl::Matches(EffectReference ref,void* caster,void* path) {
    Observed<bool> out;auto& s=*state_;
    if(s.closed || s.busy || !ref.parameters || !caster || !path)return out;
    s.busy=true;struct Exit{bool& value;~Exit(){value=false;}} exit{s.busy};
    try {
        s.Keep(ref.parameters);s.Keep(caster);s.Keep(path);
        const auto mapped=s.access.MapPath(caster,path);
        if(!mapped.call.returned || !mapped.object)throw std::runtime_error("hit effect path mapping failed");
        s.Keep(mapped.object);const auto seen=s.access.Observe(ref);auto* effect=seen.instance.object;
        if(effect)s.Keep(effect);
        EffectControlResult current;
        if(s.Current(ref,effect,current)) {
            void* original=nullptr;
            if(!native::CopyMemoryChecked(&original,static_cast<unsigned char*>(effect)+s.methods.path_offset,sizeof(original)))
                throw std::runtime_error("hit effect path unread");
            if(original)s.Keep(original);
            out.read=native::Call(s.methods.path_equal,&out.value,original,mapped.object);
        }else if(current.state==EffectControlState::NotLoaded || current.state==EffectControlState::Ended)out.read.returned=true;
    }catch(const std::exception&){}
    s.Release();return out;
}
}
