#include <hksr/presentation/effects/EffectActions.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct EffectActions::State {
    EffectAccess access;EffectActionMethods methods;ManagedObjects& objects;ManagedArrays arrays;ManagedRoots roots;
    std::vector<ManagedRootId> temporary,types;void* animator_type=nullptr;
    std::size_t request_offset=0;bool closed=false,busy=false;
    State(EffectAccess a,EffectActionMethods m,ReferenceMethods r,ManagedObjects& o,ManagedArrays ar,
        const std::vector<DeclaredField>& fields):access(std::move(a)),methods(std::move(m)),objects(o),arrays(std::move(ar)),roots(r) {
        bool found=false;
        for(const auto& field:fields)if(field.owner_namespace=="RPG.Client" && field.owner_name=="MonoEffect" && field.name=="TriggerParams") {
            if(found || field.type!="RPG.Client.TriggerEffectParams" || !field.flags_observed || (field.flags&0x10))
                throw std::invalid_argument("MonoEffect.TriggerParams declaration differs");
            request_offset=field.offset;found=true;
        }
        if(!found)throw std::invalid_argument("MonoEffect.TriggerParams declaration absent");
    }
    void Keep(void* object,std::vector<ManagedRootId>& list) {
        list.reserve(list.size()+1);list.push_back(roots.Keep(object));
        if(!roots.Status(list.back()).ready)throw std::runtime_error("effect action retention failed");
    }
    bool Release(std::vector<ManagedRootId>& list) {
        for(auto it=list.begin();it!=list.end();) {
            if(roots.Release(*it).complete){roots.Forget(*it);it=list.erase(it);}else ++it;
        }
        return list.empty();
    }
    bool Current(EffectReference ref,EffectControlResult& out,void* expected=nullptr) {
        const auto seen=access.Observe(ref);
        if(!seen.id_before.read.returned || !seen.id_after.read.returned || !seen.released.read.returned) {
            out.state=EffectControlState::Failed;out.detail="request identity unreadable";return false;
        }
        if(!seen.same_generation || seen.released.value){out.state=EffectControlState::Ended;out.detail="request released or reused";return false;}
        if(!expected)return true;
        if(!seen.instance.call.returned || seen.instance.object!=expected){out.state=EffectControlState::Failed;out.detail="request instance changed";return false;}
        void* owner=nullptr;
        if(!native::CopyMemoryChecked(&owner,static_cast<unsigned char*>(expected)+request_offset,sizeof(owner)) || owner!=ref.parameters) {
            out.state=EffectControlState::Failed;out.detail="effect request relation differs";return false;
        }
        return true;
    }
};
EffectActions::EffectActions(EffectAccess a,EffectActionMethods m,ReferenceMethods r,ManagedObjects& o,ManagedArrays ar,
    const std::vector<DeclaredField>& f):state_(new State(std::move(a),std::move(m),r,o,std::move(ar),f)){}
EffectActions::~EffectActions(){delete state_;}
EffectControlResult EffectActions::Trigger(EffectReference ref,const std::string& input,bool reset) {
    auto& s=*state_;EffectControlResult out;
    if(s.closed || s.busy || !ref.parameters || input.empty()){out.detail="closed, reentered or empty trigger input";return out;}
    const auto name=input; // a callback may replace the caller's program
    s.busy=true;struct Exit{bool& v;~Exit(){v=false;}} exit{s.busy};
    try {
        const auto method=reset?s.methods.reset_trigger:s.methods.set_trigger;
        if(!method.code || !s.methods.children.code || !s.methods.alive.code){out.state=EffectControlState::Unsupported;out.detail="trigger binding unavailable";}
        else {
            s.Keep(ref.parameters,s.temporary);
            const auto seen=s.access.Observe(ref);
            if(s.Current(ref,out)) {
                auto* effect=seen.instance.object;
                if(!seen.instance.call.returned)out.detail="effect instance unreadable";
                else if(!effect){out.state=EffectControlState::NotLoaded;out.detail="effect still loading";}
                else {
                    s.Keep(effect,s.temporary);
                    if(!s.animator_type) {
                        const auto type=s.objects.ManagedType("UnityEngine","Animator");
                        if(!type.call.returned || !type.object)throw std::runtime_error("Animator type unavailable");
                        s.Keep(type.object,s.types);s.animator_type=type.object;
                    }
                    const auto text=s.objects.NewStringUtf8(name.c_str());
                    if(!text.call.returned || !text.object)throw std::runtime_error("trigger name allocation failed");
                    s.Keep(text.object,s.temporary);
                    bool alive=false;out.call=native::Call(s.methods.alive,&alive,effect);
                    if(!out.call.returned || !alive)throw std::runtime_error("effect Unity object unavailable");
                    void* children=nullptr;out.call=native::Call(s.methods.children,&children,effect,s.animator_type,true);
                    if(!out.call.returned || !children)throw std::runtime_error("effect child Animator query failed");
                    s.Keep(children,s.temporary);
                    const auto length=s.arrays.Length(children);
                    if(!length.read.returned || length.value<0)throw std::runtime_error("Animator array length unavailable");
                    out.state=EffectControlState::Applied;std::size_t applied=0;
                    for(int i=0;i<length.value;++i) {
                        const auto child=s.arrays.Get(children,i);
                        if(!child.call.returned || !child.object)throw std::runtime_error("Animator array item unavailable");
                        s.Keep(child.object,s.temporary);
                        out.call=native::Call(s.methods.alive,&alive,child.object);
                        if(!out.call.returned || !alive)throw std::runtime_error("child Animator unavailable");
                        if(!s.Current(ref,out,effect))break;
                        out.call=native::CallVoid(method,child.object,text.object);
                        if(!out.call.returned)throw std::runtime_error("Animator trigger call failed; application may be partial");
                        ++applied;
                    }
                    if(out.state==EffectControlState::Applied)out.detail="trigger accepted by "+std::to_string(applied)+" Animators";
                }
            }
        }
    }catch(const std::exception& e){out.state=EffectControlState::Failed;out.detail=e.what();}
    out.references_released=s.Release(s.temporary);return out;
}
EffectRemovalResult EffectActions::Remove(void* manager,EffectReference ref,const EffectRemovalOptions& input) {
    auto& s=*state_;EffectRemovalResult out;
    // The official filter treats ID zero as a wildcard, not an exact receipt.
    if(s.closed || s.busy || !manager || !ref.parameters || !ref.id){out.detail="exact nonzero request ID and live service required";return out;}
    const auto options=input;s.busy=true;struct Exit{bool& v;~Exit(){v=false;}} exit{s.busy};
    void* parameters=nullptr;
    try {
        if(!s.methods.remove_input.code || !s.methods.remove.code || !s.methods.release_input.code)
            throw std::runtime_error("effect removal bindings unavailable");
        s.Keep(manager,s.temporary);s.Keep(ref.parameters,s.temporary);EffectControlResult current;
        if(s.Current(ref,current)) {
            const auto acquired=native::Call(s.methods.remove_input,&parameters,static_cast<void*>(nullptr),static_cast<void*>(nullptr),
                static_cast<void*>(nullptr),static_cast<void*>(nullptr),options.unbind,options.immediate_fade,ref.id,static_cast<void*>(nullptr),true);
            if(!acquired.returned || !parameters)throw std::runtime_error("effect removal input acquisition failed");
            s.Keep(parameters,s.temporary);
            if(s.Current(ref,current))out.removal.read=native::Call(s.methods.remove,&out.removal.value,manager,parameters,options.fade);
            else out.detail=current.detail;
        }else out.detail=current.detail;
    }catch(const std::exception& e){out.detail=e.what();}
    // Release only the temporary removal filter, never the submitted effect
    // parameters. The manager owns cancellation and any late completion.
    if(parameters)out.input_release=native::CallVoid(s.methods.release_input,parameters);
    if(parameters && !out.input_release.returned)out.detail+="; removal input release failed";
    out.references_released=s.Release(s.temporary);return out;
}
bool EffectActions::Close(){auto& s=*state_;if(s.busy)return false;s.closed=true;s.animator_type=nullptr;
    const bool a=s.Release(s.temporary),b=s.Release(s.types);return a && b;}
}
