#include <hksr/model/ModelTransition.hpp>
#include <hksr/presentation/PresentationContext.hpp>
#include <hksr/presentation/PresentationBindings.hpp>
#include <Windows.h>
#include <map>
#include <cstring>
#include <stdexcept>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<hksr::api::ModelTransition>);
namespace {
using namespace hksr::api;
int info,entity_data,world_data,other_world,model_data,type_data,animator_data;
auto* entity=reinterpret_cast<GameEntity*>(&entity_data);
auto* world=reinterpret_cast<GameWorld*>(&world_data);
std::map<int,void*> roots;
int next_id=0,type_reads=0,component_reads=0,state_reads=0,observed_hash=0;
bool fail_release=false;
std::function<void()> on_type,on_keep,on_component;
void CheckAt(bool ok,int line){if(!ok)throw std::runtime_error("model transition assertion line "+std::to_string(line));}
#define Check(value) CheckAt((value),__LINE__)
template<class T>Method M(T fn){return {reinterpret_cast<void*>(fn),&info};}
int __fastcall Keep(void* value,int,int,void*){roots[++next_id]=value;if(on_keep)on_keep();return next_id;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){if(fail_release)RaiseException(0xE0690001,0,0,nullptr);roots.erase(id);}
GameWorld* __fastcall World(void*,void*){return world;}
void* __fastcall Component(void*,void* type,void*){Check(type==&type_data);++component_reads;if(on_component)on_component();return &model_data;}
void* __fastcall Animator(void*,void*){return &animator_data;}
void __fastcall Observe(void* a,int layer,int index,unsigned char* bytes,void*) {
    Check(a==&animator_data && layer==0 && index==0);++state_reads;
    std::memset(bytes,0,36);std::memcpy(bytes,&observed_hash,4);
}
struct Objects final:ManagedObjects {
    ObjectResult ManagedType(const char* ns,const char* name) override {
        Check(std::string(ns)=="RPG.GameCore" && std::string(name)=="CharacterModelComponent");
        ++type_reads;if(on_type)on_type();return {{true,0},&type_data};
    }
    ObjectResult NewStringUtf8(const char*) override{throw std::runtime_error("unexpected string");}
    ObjectResult AllocateObject(const char*,const char*) override{throw std::runtime_error("unexpected allocation");}
};
}
void TestModelTransition() {
    using Phase=ModelTransitionPhase;
    Objects objects;
    ReferenceMethods references{M(Keep),M(Target),M(Free),{}, {}};
    EntityAccessMethods em;em.owner_world=M(World);em.component=M(Component);
    ModelAccessMethods mm;mm.animator=M(Animator);
    AnimationControlMethods am;am.state_info=M(Observe);
    PresentationBindings context_bindings;context_bindings.references=references;context_bindings.entity=em;context_bindings.model=mm;context_bindings.animation=am;
    auto transitions=[&]{PresentationContext context(objects,context_bindings);return context.ModelTransitions();}();
    auto owner=TrackedEntities().Track(entity);
    auto make=[&]{return transitions(owner,world,nullptr);};
    ModelTransitionPlan plan;plan.reload=false;
    WorldFrame frame;frame.world=world;frame.disposing={false,{true,0}};frame.sequence=100;
    auto foreign=frame;foreign.world=reinterpret_cast<GameWorld*>(&other_world);
    {
        auto value=make();value->Select(plan);
        const auto reads=type_reads;value->Service(foreign,objects,{});Check(type_reads==reads);
        bool adopted=false;int bindings=0;
        ModelTransitionConsumer c;c.adopt=[&]{return adopted;};c.events_ready=[&]{++bindings;};
        value->Service(frame,objects,{},c);Check(value->Status().phase==Phase::Adopting && type_reads==reads);
        adopted=true;value->Service(frame,objects,{},c);
        Check(value->Status().phase==Phase::Ready && bindings==1 && roots.size()==1);
        value->Service(frame,objects,{},c);Check(bindings==1);
        Check(value->Close(frame) && roots.empty());
    }
    {
        auto value=make();auto initialized=plan;initialized.initialize=true;value->Select(initialized);
        int initial_calls=0;bool ready=false;
        ModelTransitionConsumer c;c.initialize=[&]{++initial_calls;return InitialOutput{ready?InitialOutputPhase::Returned:InitialOutputPhase::Pending,41};};
        value->Service(frame,objects,{},c);Check(value->Status().phase==Phase::Initializing && initial_calls==1);
        ready=true;value->Service(frame,objects,{},c);const auto reads=state_reads;
        value->Service(frame,objects,{},c);Check(initial_calls==2 && state_reads==reads);
        ++frame.sequence;value->Service(frame,objects,{},c);
        Check(value->Status().phase==Phase::Initializing && state_reads==reads+1 && initial_calls==2);
        observed_hash=41;++frame.sequence;value->Service(frame,objects,{},c);
        Check(value->Status().phase==Phase::Ready && initial_calls==2);
        // Reusing the model is still a new output request; a prior initial
        // state's receipt must not satisfy it before another official frame.
        value->Select(initialized);value->Service(frame,objects,{},c);
        Check(value->Status().phase==Phase::Initializing && initial_calls==3);
        value->Service(frame,objects,{},c);Check(initial_calls==3);
        ++frame.sequence;value->Service(frame,objects,{},c);Check(value->Status().phase==Phase::Ready);
        Check(value->Close(frame) && roots.empty());
    }
    {
        auto value=make();auto initialized=plan;initialized.initialize=true;value->Select(initialized);
        ModelTransitionConsumer c;c.initialize=[] {return InitialOutput{InitialOutputPhase::Absent};};
        value->Service(frame,objects,{},c);
        Check(value->Status().phase==Phase::Ready && !value->Status().initialization_note.empty());
        Check(value->Close(frame));
    }
    {
        auto value=make();value->Select(plan);
        ModelTransitionConsumer c;c.adopt=[]()->bool{throw std::runtime_error("consumer failure");};
        const auto reads=type_reads;value->Service(frame,objects,{},c);
        Check(value->Status().phase==Phase::Failed && value->Status().error=="consumer failure");
        value->Service(frame,objects,{},c);Check(type_reads==reads && !TrackedEntities().IsDisposed(owner));
        value->Select(plan);value->Service(frame,objects,{});Check(value->Status().phase==Phase::Ready);
        Check(value->Close(frame));
    }
    {
        auto value=make();value->Select(plan);bool callback=false;
        on_keep=[&]{callback=true;Check(!value->Close(frame) && roots.size()==1);};
        const auto reads=component_reads;value->Service(frame,objects,{});on_keep={};
        Check(callback && component_reads==reads && value->Close(frame) && roots.empty());
    }
    {
        auto value=make();value->Select(plan);
        on_type=[&]{value->Cancel();};const auto reads=component_reads;
        value->Service(frame,objects,{});on_type={};
        Check(component_reads==reads && roots.empty() && value->Close(frame));
    }
    {
        auto value=make();value->Select(plan);int bindings=0;
        on_component=[&]{value->Select(plan);};
        ModelTransitionConsumer c;c.events_ready=[&]{++bindings;};
        value->Service(frame,objects,{},c);on_component={};
        Check(bindings==0 && value->Status().phase==Phase::Adopting);
        value->Service(frame,objects,{},c);Check(bindings==1 && value->Status().phase==Phase::Ready);
        Check(value->Close(frame));
    }
    {
        auto value=make();value->Select(plan);bool interrupt=true;int adoptions=0;
        ModelTransitionConsumer c;c.interrupted=[&]{return interrupt;};c.adopt=[&]{++adoptions;return true;};
        value->Service(frame,objects,{},c);Check(adoptions==0);
        interrupt=false;value->Service(frame,objects,{},c);Check(adoptions==1);
        Check(!value->Close(foreign) && !roots.empty());
        Check(value->Close(frame) && roots.empty());
    }
    {
        auto first=make(),second=make();first->Select(plan);second->Select(plan);
        first->Service(frame,objects,{});second->Service(frame,objects,{});Check(roots.size()==2);
        Check(first->Close(frame) && roots.size()==1 && second->Status().phase==Phase::Ready);
        Check(second->Close(frame) && roots.empty());
    }
    {
        auto value=make();value->Select(plan);value->Service(frame,objects,{});
        fail_release=true;Check(!value->Close(frame));fail_release=false;
        Check(!value->Close(frame) && roots.size()==1);
        value.reset();Check(roots.size()==1);roots.clear(); // fake fixture disposal, not SDK cleanup
    }
    {
        auto value=make();value->Select(plan);
        TrackedEntities().Disposed(owner);TrackedEntities().Forget(owner);
        const auto replacement=TrackedEntities().Track(entity);
        Check(replacement.generation!=owner.generation);
        const auto reads=component_reads;value->Service(frame,objects,{});
        Check(component_reads==reads && value->Close(frame));
        TrackedEntities().Disposed(replacement);TrackedEntities().Forget(replacement);
    }
}
