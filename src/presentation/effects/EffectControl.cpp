#include <hksr/presentation/effects/EffectControl.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace hksr::sdk_v1 {
namespace {
std::size_t RequestField(const std::vector<DeclaredField>& fields) {
    const DeclaredField* found=nullptr;
    for(const auto& field:fields)if(field.owner_namespace=="RPG.Client" &&
        field.owner_name=="MonoEffect" && field.name=="TriggerParams") {
        if(found || field.type!="RPG.Client.TriggerEffectParams" || !field.flags_observed || (field.flags&0x10))
            throw std::invalid_argument("MonoEffect.TriggerParams declaration differs");
        found=&field;
    }
    if(!found)throw std::invalid_argument("MonoEffect.TriggerParams declaration absent");
    return found->offset;
}
}
struct EffectControl::State {
    EffectAccess access;EffectControlMethods methods;ManagedObjects& objects;ManagedRoots roots;
    std::size_t request_offset;
    std::vector<ManagedRootId> temporary,types;
    void* animator_type=nullptr;void* progress_type=nullptr;
    bool closed=false,busy=false;
    State(EffectAccess a,EffectControlMethods m,ReferenceMethods refs,ManagedObjects& o,
        const std::vector<DeclaredField>& fields):access(std::move(a)),methods(std::move(m)),objects(o),roots(refs),request_offset(RequestField(fields)){}
    void Keep(void* object,std::vector<ManagedRootId>& receipts) {
        receipts.reserve(receipts.size()+1);
        const auto id=roots.Keep(object);receipts.push_back(id);
        if(!roots.Status(id).ready)throw std::runtime_error("effect control retention failed");
    }
    bool Release(std::vector<ManagedRootId>& receipts) {
        for(auto it=receipts.begin();it!=receipts.end();) {
            if(roots.Release(*it).complete){roots.Forget(*it);it=receipts.erase(it);}else ++it;
        }
        return receipts.empty();
    }
    void* Type(bool progress) {
        auto& cached=progress?progress_type:animator_type;
        if(!cached) {
            const auto value=objects.ManagedType(progress?"RPG.Client":"UnityEngine",progress?"MonoEffectPluginProgress":"Animator");
            if(!value.call.returned || !value.object)throw std::runtime_error("effect component type unavailable");
            Keep(value.object,types);cached=value.object;
        }
        return cached;
    }
    bool Current(EffectReference ref,void* expected,EffectControlResult& out) {
        const auto seen=access.Observe(ref);
        if(!seen.id_before.read.returned || !seen.id_after.read.returned || !seen.released.read.returned || !seen.instance.call.returned) {
            out.detail="request observation failed";return false;
        }
        if(!seen.same_generation || seen.released.value) {out.state=EffectControlState::Ended;out.detail="request released or reused";return false;}
        if(!seen.instance.object) {out.state=EffectControlState::NotLoaded;out.detail="request has no loaded effect";return false;}
        if(seen.instance.object!=expected) {out.detail="request effect changed during lookup";return false;}
        void* owner=nullptr;
        if(!native::CopyMemoryChecked(&owner,static_cast<unsigned char*>(expected)+request_offset,sizeof(owner))) {
            out.detail="effect request relation unreadable";return false;
        }
        if(owner!=ref.parameters){out.detail="effect belongs to a different request";return false;}
        return true;
    }
    bool Alive(void* object,EffectControlResult& out) {
        bool alive=false;out.call=native::Call(methods.alive,&alive,object);
        if(!out.call.returned){out.detail="Unity lifetime query failed";return false;}
        if(!alive){out.state=EffectControlState::Ended;out.detail="Unity object destroyed";return false;}
        return true;
    }
    EffectControlResult Apply(EffectReference ref,bool progress,int hash,float value,float duration,bool restart) {
        EffectControlResult out;
        if(closed || busy){out.detail=closed?"effect control closed":"effect control reentered";return out;}
        if(!ref.parameters || !std::isfinite(value) || !std::isfinite(duration)) {out.detail="invalid effect control input";return out;}
        busy=true;struct Exit{bool& flag;~Exit(){flag=false;}} exit{busy};
        try {
            if(!methods.component.code || !methods.alive.code ||
                (progress?(!methods.progress_behavior.code || !methods.progress_owner.code || !methods.progress_set.code):
                    !(duration>0?methods.animator_cross_fade.code:methods.animator_play.code))) {
                out.state=EffectControlState::Unsupported;out.detail="operation binding unavailable";
            }else {
                Keep(ref.parameters,temporary);
                const auto observed=access.Observe(ref);
                auto* effect=observed.instance.object;
                if(observed.instance.call.returned && effect)Keep(effect,temporary);
                // A null instance is an ordinary async state, not permission to
                // load it synchronously or select another effect with its name.
                if(Current(ref,effect,out)) {
                    if(Alive(effect,out)) {
                        auto* type=Type(progress);void* component=nullptr;
                        out.call=native::Call(methods.component,&component,effect,type);
                        if(!out.call.returned)out.detail="effect component query failed";
                        else if(!component){out.state=EffectControlState::Unsupported;out.detail="effect component absent";}
                        else {
                            Keep(component,temporary);
                            if(Alive(component,out)) {
                                void* receiver=component;bool ready=true;
                                if(progress) {
                                    out.call=native::Call(methods.progress_behavior,&receiver,component);
                                    if(!out.call.returned || !receiver){ready=false;out.detail="progress behavior unavailable";}
                                    else {
                                        Keep(receiver,temporary);void* owner=nullptr;
                                        out.call=native::Call(methods.progress_owner,&owner,receiver);
                                        ready=out.call.returned && owner==component;
                                        if(!ready)out.detail="progress behavior owner differs";
                                    }
                                }
                                // GC roots do not prevent pooling. Recheck the
                                // request identity after all managed lookups.
                                if(ready && Current(ref,effect,out)) {
                                    if(progress)out.call=native::CallVoid(methods.progress_set,receiver,value,duration);
                                    else {
                                        const float start=restart?0.0f:-std::numeric_limits<float>::infinity();
                                        out.call=duration>0?native::CallVoid(methods.animator_cross_fade,receiver,hash,duration,0,start,0.0f):
                                            native::CallVoid(methods.animator_play,receiver,hash,0,start);
                                    }
                                    out.state=out.call.returned?EffectControlState::Applied:EffectControlState::Failed;
                                    out.detail=out.call.returned?"setter returned":"setter failed; application may be partial";
                                }
                            }
                        }
                    }
                }
            }
        }catch(const std::exception& e){out.state=EffectControlState::Failed;out.detail=e.what();}
        out.references_released=Release(temporary);return out;
    }
};
EffectControl::EffectControl(EffectAccess a,EffectControlMethods m,ReferenceMethods r,ManagedObjects& o,
    const std::vector<DeclaredField>& f):state_(new State(std::move(a),std::move(m),r,o,f)){}
EffectControl::~EffectControl(){delete state_;}
EffectControlResult EffectControl::SetProgress(EffectReference r,float value,float duration){return state_->Apply(r,true,0,value,duration,false);}
EffectControlResult EffectControl::SetAnimatorState(EffectReference r,int hash,float duration,bool restart){return state_->Apply(r,false,hash,0,duration,restart);}
bool EffectControl::Close(){auto& s=*state_;if(s.busy)return false;s.closed=true;s.animator_type=nullptr;s.progress_type=nullptr;
    const bool calls=s.Release(s.temporary),types=s.Release(s.types);return calls && types;}
}
