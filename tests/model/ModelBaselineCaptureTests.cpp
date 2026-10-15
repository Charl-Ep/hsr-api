#include <hksr/model/ModelBaselineCapture.hpp>
#include <hksr/presentation/PresentationContext.hpp>
#include <hksr/presentation/PresentationBindings.hpp>
#include <hksr/control/ActorControlSession.hpp>
#include <hksr/presentation/events/AnimationEventPolicy.hpp>
#include <hksr/model/CharacterOutfit.hpp>
#include <hksr/presentation/effects/OwnedEffectRequest.hpp>
#include <Windows.h>
#include <map>
#include <functional>
#include <stdexcept>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<hksr::api::ModelBaselineCapture>);
static_assert(!std::is_copy_constructible_v<hksr::api::PresentationContext>);
static_assert(!std::is_copy_constructible_v<hksr::api::OwnedEffectRequest>);
namespace {
using namespace hksr::api;
int info,entity_data,world_data,foreign_data,model_data,type_data;
auto* entity=reinterpret_cast<GameEntity*>(&entity_data);
auto* world=reinterpret_cast<GameWorld*>(&world_data);
std::map<int,void*> roots;
int next_id=0,type_reads=0,component_reads=0,free_calls=0,load=4;
bool fail_release=false;
std::function<void()> on_keep;
void CheckAt(bool value,int line){if(!value)throw std::runtime_error("baseline capture assertion line "+std::to_string(line));}
#define Check(value) CheckAt((value),__LINE__)
template<class T>Method M(T fn){return {reinterpret_cast<void*>(fn),&info};}
int __fastcall Keep(void* value,int,int,void*){roots[++next_id]=value;if(on_keep)on_keep();return next_id;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){++free_calls;if(fail_release)RaiseException(0xE0690001,0,0,nullptr);roots.erase(id);}
GameWorld* __fastcall World(void*,void*){return world;}
void* __fastcall Component(void*,void* type,void*){Check(type==&type_data);++component_reads;return &model_data;}
int __fastcall Load(void*,void*){return load;}
struct Objects final:ManagedObjects {
    ObjectResult ManagedType(const char* ns,const char* type) override {
        Check(std::string(ns)=="RPG.GameCore" && std::string(type)=="CharacterModelComponent");
        ++type_reads;return {{true,0},&type_data};
    }
    ObjectResult NewStringUtf8(const char*) override{throw std::runtime_error("unexpected string allocation");}
    ObjectResult AllocateObject(const char*,const char*) override{throw std::runtime_error("unexpected object allocation");}
};
}
void TestModelBaselineCapture() {
    using Phase=BaselineCapturePhase;
    Objects objects;PresentationBindings b;
    b.references={M(Keep),M(Target),M(Free),{}, {}};
    b.lifetime.alive=M(+[](void*,void*)->bool{throw std::runtime_error("unexpected native object read");});
    b.lifetime.destroy=M(+[](void*,void*){throw std::runtime_error("unexpected native object destruction");});
    b.entity.owner_world=M(World);b.entity.component=M(Component);b.model.load_state=M(Load);
    PresentationContext context(objects,b);
    // Constructor makes no managed calls. Optional 4.6 inputs are not a
    // prerequisite for native binding assembly and an independent capture.
    Check(type_reads==0 && !context.Effects() && !context.Outfits());
    bool needs_runtime=false;
    try {context.CreateAnimationPolicy({});}catch(const std::logic_error&){needs_runtime=true;}
    Check(needs_runtime);
    const auto services=context.ModelRequests();
    Check(!services.event_values && services.completion!=nullptr);
    auto token=TrackedEntities().Track(entity);
    // Factory retains native bindings even after its temporary context wrapper
    // is gone. Constructing/queuing controls must not allocate managed objects.
    auto factory=[&]{PresentationContext temporary(objects,b);return temporary.Controls();}();
    auto controls=factory(token,world);
    const auto receipt=controls->Place({1,2,3},{0,0,0,1},false);
    Check(type_reads==0 && roots.empty() && controls->PlannedPlacement().position.z==3);
    controls->Cancel();Check(receipt->phase==ControlPhase::Cancelled);controls.reset();
    auto captures=[&]{PresentationContext temporary(objects,b);return temporary.Baselines();}();
    auto capture=[&]{return captures(token,world,ProfileDescription{"alternate.prefab"});};
    WorldFrame frame;frame.world=world;frame.disposing={false,{true,0}};
    auto foreign=frame;foreign.world=reinterpret_cast<GameWorld*>(&foreign_data);
    ManagedArrays arrays({});
    {
        auto value=capture();value->Service(foreign,objects,arrays,{});
        Check(type_reads==0 && roots.empty());
        value->Service(frame,objects,arrays,{});
        Check(value->Status().phase==Phase::Waiting && !value->Status().attempted && roots.size()==1);
        load=5;value->Service(frame,objects,arrays,{});
        Check(value->Status().phase==Phase::Failed && value->Status().attempted && !value->Status().errors.empty());
        const auto reads=component_reads;value->Service(frame,objects,arrays,{});Check(component_reads==reads);
        bool rejected=false;try{value->TakeRequest();}catch(const std::logic_error&){rejected=true;}Check(rejected);
        Check(value->Close() && roots.empty());
        Check(value->Close()); // no repeated free after successful release
    }
    load=4;
    {
        auto value=capture();value->Service(frame,objects,arrays,{});
        const auto frees=free_calls;fail_release=true;
        Check(!value->Close() && !value->Status().closed && roots.size()==1 && free_calls==frees+1);
        fail_release=false;
        Check(!value->Close() && roots.size()==1 && free_calls==frees+1);
        value.reset();Check(free_calls==frees+1); // destructor must not retry an unknown decrement
        roots.clear(); // discard fake fixture state, NOT claimed SDK cleanup
    }
    {
        auto value=capture();const auto reads=type_reads;
        value->Cancel();value->Service(frame,objects,arrays,{});
        Check(value->Status().phase==Phase::Cancelled && type_reads==reads && value->Close());
    }
    {
        auto value=capture();bool callback=false;
        on_keep=[&]{callback=true;Check(!value->Close() && roots.size()==1);};
        const auto reads=component_reads;value->Service(frame,objects,arrays,{});on_keep={};
        Check(callback && value->Status().phase==Phase::Cancelled && component_reads==reads);
        Check(value->Close() && roots.empty());
    }
    {
        auto value=capture();value->Service(frame,objects,arrays,{});
        TrackedEntities().Disposed(token);TrackedEntities().Forget(token);
        const auto replacement=TrackedEntities().Track(entity);
        Check(replacement.generation!=token.generation);
        const auto reads=component_reads;value->Service(frame,objects,arrays,{});
        Check(value->Status().phase==Phase::OwnerEnded && component_reads==reads);
        Check(value->Close() && roots.empty());TrackedEntities().Disposed(replacement);TrackedEntities().Forget(replacement);
    }
    Check(context.Close());
}
