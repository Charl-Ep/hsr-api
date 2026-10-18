#include <hksr/presentation/effects/EffectAnimatorParameters.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <cmath>
#include <stdexcept>
#include <type_traits>

namespace hksr::sdk_v1 {
struct EffectAnimatorParameters::State {
    EffectAccess access;EffectParameterMethods methods;ManagedObjects& objects;ManagedArrays arrays;ManagedRoots roots;
    std::vector<ManagedRootId> temporary,types;void* animator_type=nullptr;
    std::size_t request_offset=0;bool closed=false,busy=false;
    State(EffectAccess a,EffectParameterMethods m,ReferenceMethods r,ManagedObjects& o,ManagedArrays ar,
        const std::vector<DeclaredField>& fields):access(std::move(a)),methods(std::move(m)),objects(o),arrays(std::move(ar)),roots(r) {
        bool found=false;
        for(const auto& field:fields)if(field.owner_namespace=="RPG.Client" && field.owner_name=="MonoEffect" && field.name=="TriggerParams") {
            if(found || field.type!="RPG.Client.TriggerEffectParams" || !field.flags_observed || (field.flags&0x10))
                throw std::invalid_argument("MonoEffect.TriggerParams declaration differs");
            request_offset=field.offset;found=true;
        }
        if(!found)throw std::invalid_argument("MonoEffect.TriggerParams declaration absent");
    }
    void Keep(void* p,std::vector<ManagedRootId>& list) {
        list.reserve(list.size()+1);list.push_back(roots.Keep(p));
        if(!roots.Status(list.back()).ready)throw std::runtime_error("effect parameter retention failed");
    }
    bool Release(std::vector<ManagedRootId>& list) {
        for(auto it=list.begin();it!=list.end();) {
            if(roots.Release(*it).complete){roots.Forget(*it);it=list.erase(it);}else ++it;
        }
        return list.empty();
    }
    void Required(CallResult call,EffectControlResult& out,const char* message) {
        out.call=call;if(!call.returned)throw std::runtime_error(message);
    }
    void Alive(void* object,EffectControlResult& out) {
        bool value=false;Required(native::Call(methods.alive,&value,object),out,"Unity lifetime query failed");
        if(!value)throw std::runtime_error("effect or Animator destroyed");
    }
    void* Current(EffectReference ref,EffectControlResult& out,void* expected=nullptr) {
        const auto seen=access.Observe(ref);
        if(!seen.id_before.read.returned || !seen.id_after.read.returned || !seen.released.read.returned || !seen.instance.call.returned)
            throw std::runtime_error("effect request unreadable");
        if(!seen.same_generation || seen.released.value){out.state=EffectControlState::Ended;out.detail="effect request released or reused";return nullptr;}
        auto* effect=seen.instance.object;
        if(!effect){out.state=EffectControlState::NotLoaded;out.detail="effect still loading";return nullptr;}
        if(expected && effect!=expected)throw std::runtime_error("effect instance changed during parameter lookup");
        void* owner=nullptr;
        if(!native::CopyMemoryChecked(&owner,static_cast<unsigned char*>(effect)+request_offset,sizeof(owner)) || owner!=ref.parameters)
            throw std::runtime_error("effect request relation differs");
        return effect;
    }
    int Length(void* array,EffectControlResult& out) {
        if(!array)throw std::runtime_error("Animator array absent");
        Keep(array,temporary);const auto length=arrays.Length(array);
        Required(length.read,out,"Animator array length unreadable");
        if(length.value<0)throw std::runtime_error("negative Animator array length");
        return length.value;
    }
    void* Item(void* array,int index,EffectControlResult& out) {
        const auto item=arrays.Get(array,index);Required(item.call,out,"Animator array item unreadable");
        if(!item.object)throw std::runtime_error("Animator array item absent");
        Keep(item.object,temporary);return item.object;
    }
    bool Matches(void* animator,void* text,int type,EffectControlResult& out) {
        void* parameters=nullptr;Required(native::Call(methods.parameters,&parameters,animator),out,"Animator parameters unreadable");
        const auto count=Length(parameters,out);
        for(int i=0;i<count;++i) {
            auto* parameter=Item(parameters,i,out);int actual=0;
            Required(native::Call(methods.parameter_type,&actual,parameter),out,"Animator parameter type unreadable");
            if(actual!=type)continue;
            void* name=nullptr;Required(native::Call(methods.parameter_name,&name,parameter),out,"Animator parameter name unreadable");
            if(!name)continue;
            Keep(name,temporary);bool same=false;
            Required(native::Call(methods.string_equal,&same,name,text),out,"Animator parameter comparison failed");
            if(same)return true;
        }
        return false;
    }
    EffectControlResult Apply(EffectReference ref,const EffectAnimatorParameter& input) {
        EffectControlResult out;
        if(closed || busy || !ref.parameters || input.name.empty() || input.name.find('\0')!=std::string::npos) {
            out.detail="closed, reentered or invalid effect parameter input";return out;
        }
        if(input.value.valueless_by_exception() || (std::holds_alternative<float>(input.value) && !std::isfinite(std::get<float>(input.value))) ||
            (input.selection!=EffectAnimatorSelection::FirstActive && input.selection!=EffectAnimatorSelection::FirstMatchingParameter)) {
            out.detail="invalid effect parameter value or selection";return out;
        }
        const auto value=input; // callbacks can replace the caller's program
        busy=true;struct Exit{bool& flag;~Exit(){flag=false;}} exit{busy};
        try {
            Method setter;int type=0;
            std::visit([&](const auto& v){using T=std::decay_t<decltype(v)>;
                if constexpr(std::is_same_v<T,float>){setter=methods.set_float;type=1;}
                else if constexpr(std::is_same_v<T,int>){setter=methods.set_integer;type=3;}
                else if constexpr(std::is_same_v<T,bool>){setter=methods.set_bool;type=4;}
                else {setter=methods.set_trigger;type=9;}
            },value.value);
            const bool match=value.selection==EffectAnimatorSelection::FirstMatchingParameter;
            if(!setter.code || !methods.alive.code || !methods.game_object.code ||
                (match?(!methods.children.code || !methods.parameters.code || !methods.parameter_name.code || !methods.parameter_type.code || !methods.string_equal.code):!methods.first_child.code)) {
                out.state=EffectControlState::Unsupported;out.detail="effect parameter binding unavailable";
            } else {
                Keep(ref.parameters,temporary);
                if(auto* effect=Current(ref,out)) {
                    Keep(effect,temporary);Alive(effect,out);
                    void* object=nullptr;Required(native::Call(methods.game_object,&object,effect),out,"effect GameObject query failed");
                    if(!object)throw std::runtime_error("effect GameObject absent");
                    Keep(object,temporary);Alive(object,out);
                    if(!animator_type) {
                        const auto type_object=objects.ManagedType("UnityEngine","Animator");
                        Required(type_object.call,out,"Animator type unavailable");
                        if(!type_object.object)throw std::runtime_error("Animator type absent");
                        Keep(type_object.object,types);animator_type=type_object.object;
                    }
                    const auto text=objects.NewStringUtf8(value.name.c_str());
                    Required(text.call,out,"parameter name allocation failed");
                    if(!text.object)throw std::runtime_error("parameter name absent");
                    Keep(text.object,temporary);void* selected=nullptr;
                    if(!match) {
                        Required(native::Call(methods.first_child,&selected,object,animator_type,false),out,"first active Animator query failed");
                        if(selected)Keep(selected,temporary);
                    } else {
                        void* children=nullptr;Required(native::Call(methods.children,&children,object,animator_type,false),out,"active Animator query failed");
                        const auto count=Length(children,out);
                        for(int i=0;i<count;++i) {
                            auto* child=Item(children,i,out);Alive(child,out);
                            if(Matches(child,text.object,type,out)){selected=child;break;}
                        }
                    }
                    if(Current(ref,out,effect)) {
                        if(!selected){out.state=EffectControlState::NoMatch;out.detail="no matching active Animator";}
                        else {
                            Alive(selected,out);
                            if(Current(ref,out,effect)) {
                                std::visit([&](const auto& v){using T=std::decay_t<decltype(v)>;
                                    if constexpr(std::is_same_v<T,EffectParameterTrigger>)out.call=native::CallVoid(setter,selected,text.object);
                                    else out.call=native::CallVoid(setter,selected,text.object,v);
                                },value.value);
                                if(!out.call.returned)throw std::runtime_error("Animator parameter setter failed; application may be partial");
                                out.state=EffectControlState::Applied;out.detail="parameter applied to one Animator";
                            }
                        }
                    }
                }
            }
        } catch(const std::exception& e){out.state=EffectControlState::Failed;out.detail=e.what();}
        out.references_released=Release(temporary);return out;
    }
};
EffectAnimatorParameters::EffectAnimatorParameters(EffectAccess a,EffectParameterMethods m,ReferenceMethods r,ManagedObjects& o,ManagedArrays ar,
    const std::vector<DeclaredField>& fields):state_(new State(std::move(a),std::move(m),r,o,std::move(ar),fields)){}
EffectAnimatorParameters::~EffectAnimatorParameters(){delete state_;}
EffectControlResult EffectAnimatorParameters::Set(EffectReference ref,const EffectAnimatorParameter& value){return state_->Apply(ref,value);}
bool EffectAnimatorParameters::Close(){auto& s=*state_;if(s.busy)return false;s.closed=true;s.animator_type=nullptr;
    const bool a=s.Release(s.temporary),b=s.Release(s.types);return a && b;}
}
