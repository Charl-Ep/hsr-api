#include <hksr/animation/AnimationResources.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <Windows.h>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace {
using namespace hksr::api;
int entity,info,types[3],components[3],replacement,next=0;
auto* owner=reinterpret_cast<GameEntity*>(&entity);
std::map<int,void*> roots;
std::vector<std::unique_ptr<std::string>> texts;
std::function<void()> on_keep,on_type,on_component,on_free;
unsigned type_calls=0,component_calls=0,free_calls=0;
bool free_fault=false,replaced=false,type_fault=false;
std::u16string resolved_name=u"Base.阶段待机";int resolved_hash=137;bool missing_name=false;
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void Once(std::function<void()>& callback){auto fn=std::move(callback);callback={};if(fn)fn();}
template<class F>Method M(F fn){return {reinterpret_cast<void*>(fn),&info};}
void* __fastcall ResolveName(void*,int hash,void*){Check(hash==137,"requested state identity");return missing_name?nullptr:&resolved_name;}
int __fastcall NameHash(void* text,void*){Check(text==&resolved_name,"hash uses returned name");return resolved_hash;}
int __fastcall TextLength(std::u16string* text,void*){return static_cast<int>(text->size());}
std::uint16_t __fastcall TextCharacter(std::u16string* text,int index,void*){return text->at(index);}
int __fastcall Keep(void* value,int,int,void*){const auto id=++next;roots[id]=value;Once(on_keep);return id;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*) {
    ++free_calls;Once(on_free);
    if(free_fault)RaiseException(0xE0424242,0,0,nullptr);
    roots.erase(id);
}
void* __fastcall Component(void* value,void* type,void*) {
    Check(value==owner,"component owner identity");++component_calls;Once(on_component);
    for(int i=0;i<3;++i)if(type==&types[i])return replaced?&replacement:&components[i];
    return nullptr;
}
struct Objects:ManagedObjects {
    ObjectResult ManagedType(const char* ns,const char* name)override {
        ++type_calls;Once(on_type);if(type_fault)return {};
        const std::string space=ns,text=name;
        if(space=="RPG.GameCore" && text=="CharacterModelComponent")return {{true,0},&types[0]};
        if(space=="RPG.GameCore" && text=="FreeStyleComponent")return {{true,0},&types[1]};
        Check(space.empty() && text=="PEPNFLNCDHG","component adapter contract");return {{true,0},&types[2]};
    }
    ObjectResult NewStringUtf8(const char* value)override {
        texts.push_back(std::make_unique<std::string>(value));return {{true,0},texts.back().get()};
    }
    ObjectResult AllocateObject(const char*,const char*)override {throw std::runtime_error("unexpected object allocation");}
};
template<class F>void Rejected(F fn) {
    bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}
    Check(rejected,"expected operation rejected after close/owner retirement");
}
}
void TestAnimationResources() {
    static_assert(!std::is_copy_constructible_v<AnimationResources>);
    static_assert(!std::is_copy_assignable_v<AnimationResources>);
    const ReferenceMethods refs{M(Keep),M(Target),M(Free),{}, {}};
    EntityAccessMethods methods;methods.component=M(Component);EntityAccess entities(methods);
    Objects objects;auto token=TrackedEntities().Track(owner);
    {
        AnimationControlMethods am;am.resolve_hash=M(ResolveName);am.hash=M(NameHash);
        ComponentConfigMethods cm;cm.string_length=M(TextLength);cm.string_character=M(TextCharacter);
        AnimationResources names(token,refs,entities);AnimationControl actions(am);ComponentConfig text(cm);
        const auto value=names.StateName(&replacement,137,actions,text);
        Check(value.read.returned && value.utf8=="Base.阶段待机" && roots.size()==2,"resolved Unicode name owns temporary root");
        names.Drain();Check(roots.size()==1,"resolved name drained without owner");
        resolved_hash=138;Check(!names.StateName(&replacement,137,actions,text).read.returned,"mismatched name not persisted");resolved_hash=137;
        missing_name=true;Check(!names.StateName(&replacement,137,actions,text).read.returned,"null name is unavailable");missing_name=false;
        Check(names.Close() && roots.empty(),"resolved and rejected names release");
    }
    {
        AnimationResources resources(token,refs,entities);
        Check(!resources.CurrentComponent(AnimationComponent::Emotion).object && type_calls==0 && roots.empty(),"unprepared optional component allocates nothing");
        for(auto kind:{AnimationComponent::Model,AnimationComponent::FreeStyle,AnimationComponent::Emotion})
            Check(resources.Component(kind,objects).call.returned,"independent component input");
        Check(type_calls==3 && roots.size()==4,"owner and component types retained once");
        replaced=true;
        Check(resources.Component(AnimationComponent::Model,objects).object==&replacement && type_calls==3,"component identity is not cached");
        replaced=false;
        const auto text=resources.Text("动作",objects);
        Check(*static_cast<std::string*>(text)=="动作" && roots.size()==5,"text retained until drain");
        resources.Drain();Check(roots.size()==4,"drain keeps owner and types");
        const auto count=texts.size();Rejected([&]{resources.Text(std::string("A\0B",3),objects);});
        Check(texts.size()==count,"embedded null does not truncate into another animation");
        Check(resources.Close() && roots.empty(),"explicit close drains all roots");
        Rejected([&]{resources.Component(AnimationComponent::Model,objects);});
    }
    {
        AnimationResources resources(token,refs,entities);
        type_fault=true;Rejected([&]{resources.Component(AnimationComponent::Model,objects);});type_fault=false;
        Check(resources.Component(AnimationComponent::FreeStyle,objects).object==&components[1],"optional type failure does not block other components");
        Check(resources.Close() && roots.empty(),"failed type lookup cleanup");
    }
    {
        AnimationResources resources(token,refs,entities);
        const auto calls=component_calls;
        on_keep=[&]{Check(!resources.Close(),"close must defer in-flight retain");};
        Rejected([&]{resources.Component(AnimationComponent::Model,objects);});
        Check(component_calls==calls && roots.size()==1,"cancelled retain is accounted, no component call follows");
        Check(resources.Close() && roots.empty(),"deferred retained owner cleanup");
    }
    {
        AnimationResources resources(token,refs,entities);
        on_type=[&]{Rejected([&]{resources.Component(AnimationComponent::FreeStyle,objects);});};
        Check(resources.Component(AnimationComponent::Model,objects).object==&components[0],"recursive acquisition cannot corrupt outer operation");
        on_free=[&]{Check(!resources.Close(),"recursive close cannot release same handle twice");resources.Drain();};
        Check(resources.Close() && roots.empty(),"release callback reentry drains once");
    }
    {
        AnimationResources resources(token,refs,entities);
        const auto calls=component_calls;
        on_type=[&]{TrackedEntities().Disposed(token);};
        Rejected([&]{resources.Component(AnimationComponent::Model,objects);});
        Check(component_calls==calls,"no component call after type lookup retires owner");
        Check(resources.Close() && roots.empty(),"retired owner roots still close");
    }
    TrackedEntities().Forget(token);
    const auto replacement_token=TrackedEntities().Track(owner);
    Check(replacement_token.generation!=token.generation,"address reused in fixture");
    {
        AnimationResources stale(token,refs,entities);
        const auto count=type_calls;Rejected([&]{stale.Component(AnimationComponent::Model,objects);});
        Check(type_calls==count && stale.Close(),"stale epoch rejected before engine lookup");
    }
    {
        AnimationResources resources(replacement_token,refs,entities);resources.RetainOwner();
        free_fault=true;const auto calls=free_calls;
        Check(!resources.Close() && !resources.Close() && free_calls==calls+1,"unknown release is not retried");
        free_fault=false;
    }
    roots.clear(); // fake handle with unknown release; no production retry
    const auto before=free_calls;
    {
        AnimationResources resources(replacement_token,refs,entities);resources.RetainOwner();
    }
    Check(free_calls==before,"destructor never invokes engine");
    roots.clear(); // explicit fixture cleanup for no-engine destructor test
    TrackedEntities().Disposed(replacement_token);TrackedEntities().Forget(replacement_token);
}
