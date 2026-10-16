#include <hksr/control/ActorControlSession.hpp>
#include <hksr/control/ActorControlBindings.hpp>
#include <hksr/presentation/PresentationBindings.hpp>
#include <functional>
#include <limits>
#include <type_traits>
#include <hksr/entity/LifecycleHooks.hpp>
#include <Windows.h>
#include <map>
#include <stdexcept>
#include <iostream>

namespace {
using namespace hksr;
int info,world,controller_type,driver_type,model_type,head_name,look_type,next=0;
struct Node {api::Position3 p;api::Rotation4 q;bool alive=true;};
struct Actor {Node root,head,look;bool automatic=true;void* target=nullptr;bool forbid_ground=false,navigation_only=false,navigation_ground=false;};
Actor actor,other;Node anchor;
struct BattleLook {Node native;Node* head=nullptr;void* target=nullptr;bool enabled=false,active=false;};
BattleLook battle_look,old_look,new_look;
Node changed_head;
std::vector<BattleLook*> battle_looks;
bool owner_mono_absent=false;
bool root_available=true;
Node* current_head=nullptr;
unsigned enumerations=0,ik_follows=0,ik_stops=0;
bool fade_pending=false;
std::map<int,void*> roots;
std::function<void()> on_keep,on_type,on_follow,on_stop;
unsigned component_calls=0,free_calls=0;
bool fail_free=false,fail_place=false,fail_point=false,target_mono_absent=false;
bool simulate_ground=false,fail_ground=false;float motion_y=0;
void Once(std::function<void()>& callback) {auto fn=std::move(callback);callback={};if(fn)fn();}
unsigned flushes=0,places=0,follows=0,stops=0,allocations=0;
void CheckAt(bool v,int line){if(!v){std::cerr<<"character controls assertion line "<<line<<'\n';throw std::runtime_error("character controls test failed");}}
#define Check(value) CheckAt((value),__LINE__)
template<class F> api::Method M(F f){return {reinterpret_cast<void*>(f),&info};}
std::int32_t __fastcall Keep(void* p,int,int,void*){const int id=++next;roots[id]=p;Once(on_keep);return id;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){++free_calls;if(fail_free)RaiseException(0xE0424242,0,0,nullptr);roots.erase(id);}
api::GameWorld* __fastcall World(void*,void*){return reinterpret_cast<api::GameWorld*>(&world);}
bool __fastcall Loaded(void*,void*){return true;}
void* __fastcall Component(void* value,void* type,void*){++component_calls;Check(type==&controller_type || type==&driver_type || type==&model_type);return value;}
void* __fastcall GameObject(Actor* value,void*){return root_available?&value->root:nullptr;}
void* __fastcall Transform(void* value,void*){return value;}
void __fastcall Position(Node* value,api::Position3* out,void*){*out=value->p;}
void __fastcall Rotation(Node* value,api::Rotation4* out,void*){*out=value->q;}
__declspec(noinline) void __fastcall Flush(Actor* value,void*){
    if(simulate_ground) {
        value->root.p.y+=motion_y;
        if(!value->navigation_only || value->navigation_ground)value->root.p.y=0;
    }else value->root.p={-9,-8,-7};
    ++flushes;MemoryBarrier();
}
void __fastcall Place(Actor* value,const api::Position3* p,const api::Rotation4* q,void*){Check(flushes>0);if(fail_place)RaiseException(0xE0424242,0,0,nullptr);value->root.p=*p;value->root.q=*q;if(simulate_ground && !value->forbid_ground)value->root.p.y=0;++places;}
bool __fastcall GetForbidGround(Actor* value,void*) {return value->forbid_ground;}
void __fastcall SetForbidGround(Actor* value,bool forbid,void*) {if(fail_ground)RaiseException(0xE0424242,0,0,nullptr);value->forbid_ground=forbid;}
bool __fastcall GetNavigationOnly(Actor* value,void*) {return value->navigation_only;}
void __fastcall SetNavigationOnly(Actor* value,bool flag,void*) {value->navigation_only=flag;}
bool __fastcall GetNavigationGround(Actor* value,void*) {return value->navigation_ground;}
void __fastcall SetNavigationGround(Actor* value,bool flag,void*) {value->navigation_ground=flag;}
void* __fastcall Mono(void* value,void*){return (target_mono_absent && value==&other) || (owner_mono_absent && value==&actor)?nullptr:value;}
void* __fastcall Head(Actor* value,void*){return &value->head;}
void* __fastcall ModelHead(Actor* value,void* name,bool fallback,void*){Check(name==&head_name && !fallback);return value==&actor && current_head?current_head:&value->head;}
void* __fastcall Components(Node* root,void* type,bool inactive,void*) {
    Check(root==&actor.root && type==&look_type && inactive);++enumerations;return &battle_looks;
}
int __fastcall Length(std::vector<BattleLook*>* list,void*){return static_cast<int>(list->size());}
void* __fastcall Item(std::vector<BattleLook*>* list,int index,void*){return list->at(index);}
void* __fastcall IkHead(BattleLook* value,void*){return value->head;}
void __fastcall IkFollow(BattleLook* value,void* target,bool force,bool second,void* constraint,void*) {
    Check(force && !second && !constraint && value->enabled);value->active=true;value->target=target;++ik_follows;Once(on_follow);
}
void __fastcall IkStop(BattleLook* value,bool immediate,void*){Check(!immediate);value->target=nullptr;value->active=fade_pending;++ik_stops;Once(on_stop);}
bool __fastcall IkActive(BattleLook* value,void*){return value->active;}
bool __fastcall BehaviourEnabled(BattleLook* value,void*){return value->enabled;}
void __fastcall SetBehaviourEnabled(BattleLook* value,bool enabled,void*){value->enabled=enabled;}
void* __fastcall Look(Actor* value,void*){return &value->look;}
bool __fastcall Automatic(Actor* value,void*){return value->automatic;}
void __fastcall SetAutomatic(Actor* value,bool enabled,void*){value->automatic=enabled;}
void __fastcall Follow(Actor* value,void* target,bool force,bool second,void* constraint,void*){Check(force && !second && !constraint);value->target=target;++follows;Once(on_follow);}
void __fastcall Stop(Actor* value,bool second,void*){Check(!second);value->target=nullptr;++stops;Once(on_stop);}
void __fastcall Point(Node* value,const api::Position3* p,void*){if(fail_point)RaiseException(0xE0424242,0,0,nullptr);value->p=*p;}
bool __fastcall Alive(Node* value,void*){return value->alive;}
void __fastcall Destroy(Node* value,void*){value->alive=false;}
void __fastcall Initialize(Node* value,void*){value->alive=true;}
struct Objects:api::ManagedObjects {
    api::ObjectResult ManagedType(const char* ns,const char* name) override {
        Once(on_type);
        if(std::string(ns)=="RPG.GameCore" && std::string(name)=="AdventureCharacterController")return {{true,0},&controller_type};
        if(std::string(ns)=="RPG.GameCore" && std::string(name)=="CharacterModelComponent")return {{true,0},&model_type};
        if(std::string(ns)=="RPG.Client" && std::string(name)=="LookAtIK")return {{true,0},&look_type};
        Check(std::string(name)=="PKBOEALAKCM");return {{true,0},&driver_type};
    }
    api::ObjectResult AllocateObject(const char* ns,const char* type) override {Check(std::string(ns)=="UnityEngine" && std::string(type)=="GameObject");++allocations;return {{true,0},&anchor};}
    api::ObjectResult NewStringUtf8(const char* text) override {Check(std::string(text)=="Head");return {{true,0},&head_name};}
};
}
void TestActorControlSession() {
    static_assert(!std::is_copy_constructible_v<api::ActorControlSession>);
    static_assert(!std::is_copy_assignable_v<api::ActorControlSession>);
    api::PresentationBindings b;
    b.references={M(Keep),M(Target),M(Free),{}, {}};b.lifetime={M(Alive),M(Destroy)};
    b.entity.owner_world=M(World);b.entity.loaded=M(Loaded);b.entity.component=M(Component);
    b.placement.game_object=M(GameObject);b.placement.transform=M(Transform);b.placement.position=M(Position);b.placement.rotation=M(Rotation);
    b.actor_control={M(Flush),M(Place),M(Mono),M(Automatic),M(SetAutomatic),M(Head),M(Look),M(Follow),M(Stop),M(Point),{}};
    b.actor_control.model_attach_point=M(ModelHead);
    b.actor_control.forbid_ground=M(SetForbidGround);b.actor_control.get_forbid_ground=M(GetForbidGround);
    b.actor_control.navigation_only=M(SetNavigationOnly);b.actor_control.get_navigation_only=M(GetNavigationOnly);
    b.actor_control.navigation_ground=M(SetNavigationGround);b.actor_control.get_navigation_ground=M(GetNavigationGround);
    b.actor_control.ik_head=M(IkHead);b.actor_control.ik_follow=M(IkFollow);b.actor_control.ik_stop=M(IkStop);
    b.actor_control.ik_active=M(IkActive);b.actor_control.behaviour_enabled=M(BehaviourEnabled);b.actor_control.set_behaviour_enabled=M(SetBehaviourEnabled);
    b.arrays.length=M(Length);b.arrays.get=M(Item);b.scene.components=M(Components);
    b.scene.transform=M(Transform);b.visual.initialize=M(Initialize);
    Objects objects;api::SceneObjects scene(b.scene);api::VisualObjects visuals(b.visual);
    api::ActorControlServices services{objects,scene,visuals};
    api::WorldFrame frame;frame.world=reinterpret_cast<api::GameWorld*>(&world);frame.disposing={false,{true,0}};
    auto token=api::TrackedEntities().Track(reinterpret_cast<api::GameEntity*>(&actor));
    api::InstallMovementPlacement(b.actor_control);
    api::ActorControlSession controls(token,frame.world,{b.references,b.lifetime,b.entity,b.actor_control,b.placement});
    api::PlayerPlacement fallback;fallback.ready=true;fallback.position={4,5,6};fallback.rotation={0,0,0,1};
    const auto replaced=controls.Place({1,2,3},{0,0,0,1},true);
    Check(controls.PlannedPlacement(fallback).position.x==5 && controls.PlannedPlacement(fallback).position.z==9);
    const auto absolute=controls.Place({10,20,30},{0,0,0,1},false);
    Check(controls.PlannedPlacement().ready && controls.PlannedPlacement().position.x==10);
    root_available=false;
    controls.Service(frame,services,nullptr,false);
    Check(controls.Status().placement_pending && absolute->phase!=api::ControlPhase::Failed && places==0);
    Check(controls.PlannedPlacement(fallback).position.x==10);
    root_available=true;
    controls.Service(frame,services,nullptr,false);
    Check(controls.Status().placement_pending && places==0);
    controls.Service(frame,services,nullptr,true);
    Check(replaced->phase==api::ControlPhase::Superseded && absolute->phase==api::ControlPhase::Running);
    Check(places==0 && controls.Status().placement_pending);
    Flush(&other,&info);Check(places==0);
    Flush(&actor,&info);Check(places==1 && actor.root.p.x==10 && actor.root.p.z==30);
    controls.Service(frame,services,nullptr,true);Check(!controls.Status().placement_pending && controls.Status().live.position.y==20);
    Check(absolute->phase==api::ControlPhase::Returned);
    const auto offset1=controls.Place({1,2,3},{0,1,0,0},true);controls.Service(frame,services,nullptr,true);
    const auto offset2=controls.Place({2,0,0},{0,0,0,1},true);
    Check(controls.PlannedPlacement().position.x==13 && controls.PlannedPlacement().rotation.y==1);
    controls.Service(frame,services,nullptr,true);
    Flush(&actor,&info);Check(places==2 && actor.root.p.x==13 && actor.root.p.y==22 && actor.root.q.y==1);
    controls.Look({api::GazeMode::Point,{2,3,4}});controls.Service(frame,services,nullptr,true);
    Check(offset1->phase==api::ControlPhase::Returned && offset2->phase==api::ControlPhase::Returned && offset1->id!=offset2->id);
    Check(!actor.automatic && actor.target==&anchor && anchor.p.z==4 && follows==1 && allocations==1);
    controls.Look({api::GazeMode::Point,{5,6,7}});controls.Service(frame,services,nullptr,true);
    Check(anchor.p.z==7 && allocations==1);
    const auto count=follows;controls.Service(frame,services,nullptr,true);Check(follows==count);
    controls.Look({api::GazeMode::Entity,{}});controls.Service(frame,services,reinterpret_cast<api::GameEntity*>(&other),true);
    Check(actor.target==&other.head);
    target_mono_absent=true;
    controls.Look({api::GazeMode::Entity,{}});controls.Service(frame,services,reinterpret_cast<api::GameEntity*>(&other),true);
    Check(actor.target==&other.head && controls.Status().gaze==api::GazePhase::Following); // battle target has no adventure Mono
    target_mono_absent=false;
    controls.Look({api::GazeMode::Entity,{},5});other.root.p={100,0,0};
    controls.Service(frame,services,reinterpret_cast<api::GameEntity*>(&other),true);
    Check(!actor.target && controls.Status().gaze==api::GazePhase::OutOfRange);
    other.root.p=actor.root.p;controls.Service(frame,services,reinterpret_cast<api::GameEntity*>(&other),true);
    Check(actor.target==&other.head && controls.Status().gaze==api::GazePhase::Following);
    other.root.p={100,0,0};controls.Look({api::GazeMode::Entity,{}});
    controls.Service(frame,services,reinterpret_cast<api::GameEntity*>(&other),true);Check(actor.target==&other.head);
    controls.Service(frame,services,nullptr,true);Check(actor.target==nullptr); // target removed
    controls.Look({api::GazeMode::Automatic,{}});controls.Service(frame,services,nullptr,true);Check(actor.automatic);
    controls.Look({api::GazeMode::Disabled,{}});controls.Service(frame,services,nullptr,true);Check(!actor.automatic);
    Check(controls.Suspend()==api::ControlPhase::Returned && actor.automatic);
    const auto cancelled=controls.Place({99,0,0},{0,0,0,1},false);controls.Service(frame,services,nullptr,true);
    controls.Cancel();Check(controls.PlannedPlacement().position.x==13);Flush(&actor,&info);Check(places==2);
    Check(cancelled->phase==api::ControlPhase::Cancelled && offset1->phase==api::ControlPhase::Returned);
    Check(!controls.Close(frame) && !anchor.alive); // Destroy is observed on a later service
    Check(controls.Close(frame) && roots.empty());
    api::TrackedEntities().Disposed(token);api::TrackedEntities().Forget(token);

    // A non-Framework consumer owns the complete control session. No target
    // lookup or type retention occurs on the wrong world's delivery.
    token=api::TrackedEntities().Track(reinterpret_cast<api::GameEntity*>(&actor));
    {
        simulate_ground=true;
        api::ActorControlSession session(token,frame.world,{b.references,b.lifetime,b.entity,b.actor_control,b.placement});
        const auto lift=session.Place({1,2,3},{0,0,0,1},false,false);
        session.Service(frame,services,nullptr,true);Check(!actor.forbid_ground);
        Flush(&actor,&info);session.Service(frame,services,nullptr,true);
        Check(actor.forbid_ground && actor.root.p.y==2 && lift->phase==api::ControlPhase::Returned);
        // Regression: ForbidStandOnGround alone only affects the immediate
        // setter. Subsequent official movement needs its separate route.
        Check(actor.navigation_only && !actor.navigation_ground);
        Flush(&actor,&info);session.Service(frame,services,nullptr,true);Check(actor.root.p.y==2);
        motion_y=0.25f;Flush(&actor,&info);motion_y=0;
        session.Service(frame,services,nullptr,true);Check(actor.root.p.y==2.25f); // not a frozen Y
        const auto snap=session.Place({0,0,0},{0,0,0,1},true,true);
        session.Service(frame,services,nullptr,true);Flush(&actor,&info);session.Service(frame,services,nullptr,true);
        Check(!actor.forbid_ground && !actor.navigation_only && !actor.navigation_ground && actor.root.p.y==0 && snap->phase==api::ControlPhase::Returned);
        // A rapid Y step replaces pending snap intent before the same flush.
        session.Place({0,0,0},{0,0,0,1},true,true);session.Service(frame,services,nullptr,true);
        session.Place({0,3,0},{0,0,0,1},true,false);session.Service(frame,services,nullptr,true);
        Flush(&actor,&info);session.Service(frame,services,nullptr,true);Check(actor.forbid_ground && actor.root.p.y==3);
        Check(session.Suspend()==api::ControlPhase::Returned && actor.forbid_ground); // model switch preserves placement policy
        auto bad=session.Place({0,1,0},{0,0,0,1},true,true);session.Service(frame,services,nullptr,true);
        const auto before=places;fail_ground=true;Flush(&actor,&info);fail_ground=false;
        session.Service(frame,services,nullptr,true);Check(places==before && bad->phase==api::ControlPhase::Failed);
        Check(session.Close(frame) && !actor.forbid_ground && !actor.navigation_only && !actor.navigation_ground && roots.empty());
        simulate_ground=false;
    }
    {
        api::ActorControlSession session(token,frame.world,{b.references,b.lifetime,b.entity,b.actor_control,b.placement});
        auto wrong=frame;int alien;wrong.world=reinterpret_cast<api::GameWorld*>(&alien);
        session.Look({api::GazeMode::Entity,{}});
        const auto before=component_calls;
        session.Service(wrong,services,reinterpret_cast<api::GameEntity*>(&other),true);
        Check(component_calls==before && roots.empty());
        session.Service(frame,services,reinterpret_cast<api::GameEntity*>(&other),true);
        Check(session.Status().gaze==api::GazePhase::Following);
        Check(!session.Close(wrong) && !roots.empty());
        on_stop=[&]{Check(!session.Close(frame));};
        Check(session.Close(frame) && roots.empty() && actor.automatic);
    }
    // Cancel/Close from managed callbacks must not free roots currently in use,
    // or proceed from a type result to a component lookup.
    {
        api::ActorControlSession session(token,frame.world,{b.references,b.lifetime,b.entity,b.actor_control,b.placement});
        session.Look({api::GazeMode::Disabled,{}});
        const auto before=component_calls;
        on_type=[&]{session.Cancel();};
        session.Service(frame,services,nullptr,true);
        Check(component_calls==before && roots.empty());
        Check(session.Close(frame));
    }
    {
        api::ActorControlSession session(token,frame.world,{b.references,b.lifetime,b.entity,b.actor_control,b.placement});
        session.Look({api::GazeMode::Disabled,{}});
        const auto before=component_calls;
        on_keep=[&]{Check(!session.Close(frame));Check(!roots.empty());};
        session.Service(frame,services,nullptr,true);
        Check(component_calls==before && !roots.empty());
        Check(session.Close(frame) && roots.empty());
    }
    // A newer point queued synchronously during Follow survives the old call's
    // completion; the next service updates it without a new point object.
    {
        api::ActorControlSession session(token,frame.world,{b.references,b.lifetime,b.entity,b.actor_control,b.placement});
        const auto first=session.Look({api::GazeMode::Point,{1,2,3}});
        api::ControlReceipt second;
        on_follow=[&]{second=session.Look({api::GazeMode::Point,{7,8,9}});};
        session.Service(frame,services,nullptr,true);Check(anchor.p.x==1);
        Check(first->phase==api::ControlPhase::Returned && second->phase==api::ControlPhase::Waiting);
        session.Service(frame,services,nullptr,true);Check(anchor.p.x==7);
        Check(second->phase==api::ControlPhase::Returned);
        bool invalid=false;try{session.Look({api::GazeMode::Point,{std::numeric_limits<float>::infinity(),0,0}});}catch(const std::exception&){invalid=true;}
        Check(invalid);
        Check(!session.Close(frame));Check(session.Close(frame) && roots.empty());
    }
    // Target absence is waiting, not successful Stop. A new unsent intent
    // supersedes only the prior pending receipt, never its returned history.
    {
        api::ActorControlSession session(token,frame.world,{b.references,b.lifetime,b.entity,b.actor_control,b.placement});
        const auto missing=session.Look({api::GazeMode::Entity,{}});
        session.Service(frame,services,nullptr,true);Check(missing->phase==api::ControlPhase::Waiting);
        const auto automatic=session.Look({api::GazeMode::Automatic,{}});
        Check(missing->phase==api::ControlPhase::Superseded);
        session.Service(frame,services,nullptr,true);Check(automatic->phase==api::ControlPhase::Returned);
        const auto waiting=session.Look({api::GazeMode::Entity,{}});
        session.Service(frame,services,nullptr,false);Check(waiting->phase==api::ControlPhase::Waiting);
        session.Service(frame,services,reinterpret_cast<api::GameEntity*>(&other),true);
        Check(waiting->phase==api::ControlPhase::Returned);
        session.Service(frame,services,nullptr,true);Check(waiting->phase==api::ControlPhase::Returned);
        Check(session.Close(frame) && roots.empty());
    }
    // Requests submitted during component lookup are a later batch. Cancel
    // after a completed movement call must not erase its successful receipt.
    {
        api::ActorControlSession session(token,frame.world,{b.references,b.lifetime,b.entity,b.actor_control,b.placement});
        const auto first=session.Place({4,5,6},{0,0,0,1},false);
        api::ControlReceipt second;
        on_type=[&]{second=session.Place({1,0,0},{0,0,0,1},true);};
        session.Service(frame,services,nullptr,true);
        Check(first->phase==api::ControlPhase::Running && second->phase==api::ControlPhase::Waiting);
        Flush(&actor,&info);
        session.Cancel();
        Check(first->phase==api::ControlPhase::Returned && second->phase==api::ControlPhase::Cancelled);
        Check(session.Close(frame) && roots.empty());
    }
    // Stale owner tokens must never reach a reused entity's components.
    {
        api::ActorControlSession session(token,frame.world,{b.references,b.lifetime,b.entity,b.actor_control,b.placement});
        const auto first=session.Place({1,2,3},{0,0,0,1},false);session.Service(frame,services,nullptr,true);
        const auto second=session.Place({4,5,6},{0,0,0,1},false);session.Service(frame,services,nullptr,true);
        Check(first->phase==api::ControlPhase::Superseded && second->phase==api::ControlPhase::Running);
        fail_place=true;Flush(&actor,&info);fail_place=false;session.Service(frame,services,nullptr,true);
        Check(second->phase==api::ControlPhase::Failed && !second->detail.empty());
        Check(session.PlannedPlacement(fallback).position.x==fallback.position.x);
        const auto third=session.Place({7,8,9},{0,0,0,1},false);session.Service(frame,services,nullptr,true);
        Flush(&actor,&info);
        const auto delta=session.Place({1,0,0},{0,0,0,1},true);
        Check(session.PlannedPlacement().position.x==8);
        // A later absolute request cannot supersede an already executed write,
        // even when the next Service has not consumed that write yet.
        const auto fourth=session.Place({10,11,12},{0,0,0,1},false);
        Check(third->phase==api::ControlPhase::Returned && fourth->phase==api::ControlPhase::Waiting);
        const auto gaze=session.Look({api::GazeMode::Point,{3,2,1}});
        fail_point=true;session.Service(frame,services,nullptr,true);fail_point=false;
        Check(delta->phase==api::ControlPhase::Superseded);
        Check(gaze->phase==api::ControlPhase::Failed);
        session.Service(frame,services,nullptr,true);Check(gaze->phase==api::ControlPhase::Failed); // retained outcome, not current policy state
        session.Cancel();Check(fourth->phase==api::ControlPhase::Cancelled);
        Check(!session.Close(frame));Check(session.Close(frame) && roots.empty());
    }
    {
        api::ActorControlSession session(token,frame.world,{b.references,b.lifetime,b.entity,b.actor_control,b.placement});
        session.Place({4,5,6},{0,0,0,1},false);
        session.Service(frame,services,nullptr,true);
        const auto before=places;
        api::TrackedEntities().Disposed(token);api::TrackedEntities().Forget(token);
        token=api::TrackedEntities().Track(reinterpret_cast<api::GameEntity*>(&actor));
        Flush(&actor,&info);Check(places==before);
        Check(session.Close(frame) && roots.empty());
    }
    // Battle root has no MonoAvatar; choose the existing official IK by its
    // current model head, not list order, an old rig or a matching name.
    {
        owner_mono_absent=true;current_head=&actor.head;
        old_look.head=&other.head;battle_look.head=&actor.head;new_look.head=&changed_head;
        battle_looks={&old_look,&battle_look,&new_look};const auto scans=enumerations;
        api::ActorControlSession session(token,frame.world,{b.references,b.lifetime,b.entity,b.actor_control,b.placement,b.arrays});
        const auto request=session.Look({api::GazeMode::Entity,{}});
        session.Service(frame,services,reinterpret_cast<api::GameEntity*>(&other),true);
        Check(request->phase==api::ControlPhase::Returned && battle_look.target==&other.head && !old_look.target && !actor.automatic && battle_look.enabled);
        session.Service(frame,services,reinterpret_cast<api::GameEntity*>(&other),true);
        session.Look({api::GazeMode::Entity,{}});session.Service(frame,services,reinterpret_cast<api::GameEntity*>(&other),true);
        Check(enumerations==scans+1); // no hierarchy enumeration per frame/target update
        const auto stops_before=ik_stops;current_head=&changed_head;
        session.Service(frame,services,reinterpret_cast<api::GameEntity*>(&other),true);
        Check(ik_stops==stops_before+1 && !battle_look.target && !battle_look.enabled && new_look.target==&other.head);
        fade_pending=true;Check(session.Suspend()==api::ControlPhase::Waiting && new_look.enabled && !actor.automatic);
        const auto follows_before_suspend=ik_follows;
        session.Service(frame,services,reinterpret_cast<api::GameEntity*>(&other),false);
        Check(session.Status().gaze==api::GazePhase::Restoring && session.Status().gaze_error.empty() && ik_follows==follows_before_suspend);
        new_look.active=false;fade_pending=false;
        Check(session.Suspend()==api::ControlPhase::Returned && !new_look.target && !new_look.enabled && actor.automatic);
        owner_mono_absent=false;current_head=nullptr;
        session.Service(frame,services,reinterpret_cast<api::GameEntity*>(&other),true);
        Check(actor.target==&other.head && !battle_look.target && !new_look.target);
        Check(session.Close(frame) && roots.empty() && actor.automatic);
    }
    {
        owner_mono_absent=true;current_head=&actor.head;
        old_look.head=&actor.head;battle_looks={&old_look,&battle_look};
        const auto before=ik_follows;
        api::ActorControlSession session(token,frame.world,{b.references,b.lifetime,b.entity,b.actor_control,b.placement,b.arrays});
        session.Look({api::GazeMode::Entity,{}});session.Service(frame,services,reinterpret_cast<api::GameEntity*>(&other),true);
        Check(session.Status().gaze==api::GazePhase::Failed && ik_follows==before && actor.automatic);
        Check(session.Close(frame) && roots.empty());
    }
    {
        // The root component may finish binding its head after model loading.
        // Re-read the existing candidate without retaining another array/frame.
        battle_look.head=nullptr;battle_looks={&battle_look};const auto scans=enumerations;
        api::ActorControlSession session(token,frame.world,{b.references,b.lifetime,b.entity,b.actor_control,b.placement,b.arrays});
        session.Look({api::GazeMode::Entity,{}});session.Service(frame,services,reinterpret_cast<api::GameEntity*>(&other),true);
        Check(session.Status().gaze==api::GazePhase::Failed);
        battle_look.head=&actor.head;session.Service(frame,services,reinterpret_cast<api::GameEntity*>(&other),true);
        Check(session.Status().gaze==api::GazePhase::Following && enumerations==scans+1);
        on_follow=[&]{session.Cancel();};session.Look({api::GazeMode::Point,{1,2,3}});
        session.Service(frame,services,nullptr,true);
        Check(session.Status().gaze==api::GazePhase::Failed);
        Check(!session.Close(frame));Check(session.Close(frame) && roots.empty() && !battle_look.target);
        owner_mono_absent=false;current_head=nullptr;
    }
    {
        // A Behaviour already scheduled by Unity is not ours to disable.
        owner_mono_absent=true;current_head=&actor.head;battle_look.enabled=true;battle_looks={&battle_look};
        api::ActorControlSession session(token,frame.world,{b.references,b.lifetime,b.entity,b.actor_control,b.placement,b.arrays});
        session.Look({api::GazeMode::Entity,{}});session.Service(frame,services,reinterpret_cast<api::GameEntity*>(&other),true);
        Check(session.Close(frame) && battle_look.enabled && roots.empty());
        battle_look.enabled=false;owner_mono_absent=false;current_head=nullptr;
    }
    {
        // Automatic restoration is a multi-frame operation. Its receipt must
        // stay pending during official fading, then finish, never fail early.
        owner_mono_absent=true;current_head=&actor.head;battle_looks={&battle_look};
        api::ActorControlSession session(token,frame.world,{b.references,b.lifetime,b.entity,b.actor_control,b.placement,b.arrays});
        session.Look({api::GazeMode::Entity,{}});session.Service(frame,services,reinterpret_cast<api::GameEntity*>(&other),true);
        fade_pending=true;const auto automatic=session.Look({api::GazeMode::Automatic,{}});
        const auto stops_before=ik_stops;
        session.Service(frame,services,nullptr,true);
        Check(automatic->phase==api::ControlPhase::Waiting && !automatic->Terminal());
        Check(session.Status().gaze==api::GazePhase::Restoring && session.Status().gaze_error.empty() && battle_look.enabled);
        session.Service(frame,services,nullptr,true);
        Check(automatic->phase==api::ControlPhase::Waiting && ik_stops==stops_before+1);
        battle_look.active=false;fade_pending=false;session.Service(frame,services,nullptr,true);
        Check(automatic->phase==api::ControlPhase::Returned && !battle_look.enabled && actor.automatic);
        Check(session.Close(frame) && roots.empty());owner_mono_absent=false;current_head=nullptr;
    }
    // A release with unknown outcome is reported, never repeated as if safe.
    {
        api::ActorControlSession session(token,frame.world,{b.references,b.lifetime,b.entity,b.actor_control,b.placement});
        session.Look({api::GazeMode::Disabled,{}});
        session.Service(frame,services,nullptr,true);
        fail_free=true;Check(!session.Close(frame));const auto before=free_calls;
        Check(!session.Close(frame) && free_calls==before);
        fail_free=false;
    }
    roots.clear(); // synthetic handles whose mocked release deliberately failed
    api::TrackedEntities().Disposed(token);api::TrackedEntities().Forget(token);
    api::RemoveMovementPlacementAtQuiescentPoint();
    token=api::TrackedEntities().Track(reinterpret_cast<api::GameEntity*>(&actor));
    {
        api::ActorControlSession session(token,frame.world,{b.references,b.lifetime,b.entity,b.actor_control,b.placement});
        const auto no_hook=session.Place({1,2,3},{0,0,0,1},false);
        session.Service(frame,services,nullptr,true);
        Check(no_hook->phase==api::ControlPhase::Failed && !no_hook->detail.empty());
        Check(session.Close(frame) && roots.empty());
    }
    api::TrackedEntities().Disposed(token);api::TrackedEntities().Forget(token);
    std::cout<<"SDK control session passed: actual post-flush hook, actor isolation, combined offsets, cancellation, live pose, reusable point, target loss and policy restoration; local only.\n";
}
