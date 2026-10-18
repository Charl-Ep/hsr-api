#include <hksr/control/PartnerControlSession.hpp>
#include <hksr/entity/NpcCreationSession.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#include <Windows.h>
#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>

namespace {
using namespace hksr::api;
void Check(bool value){if(!value)throw std::runtime_error("partner control test failed");}
struct Component {void* header[2]{};GameEntity* owner=nullptr;};
struct Actor {GameWorld* world=nullptr;std::array<Component,5> components;};
int world_data,other_world_data,manager_data,definition_data,row_data,info;
auto* world=reinterpret_cast<GameWorld*>(&world_data);
auto* other_world=reinterpret_cast<GameWorld*>(&other_world_data);
Actor parent,child,other;
GameEntity* E(Actor& value){return reinterpret_cast<GameEntity*>(&value);}
Actor* next_actor=nullptr;
GameEntity* leader=nullptr;
std::array<int,5> types{};
std::map<int,void*> roots;
int root_id=0,member_calls=0,attach_calls=0,cancel_calls=0,anim_calls=0,visible_calls=0;
int visibility_sets=0;
int idle_calls=0;bool fault_idle=false;
PartnerIdleAnimation expected_idle;
std::map<std::string,std::array<bool,3>> hide_requests;
bool fault_attach=false,fault_cancel=false;
std::function<void()> during_string,during_attach;
std::string member_name;
GameEntity* anim_target=nullptr;
GameEntity* visibility_target=nullptr;
VisibilityFollow visibility_flag=VisibilityFollow::None;
template<class F>Method M(F f){return {reinterpret_cast<void*>(f),&info};}
bool Kept(void* value){for(const auto& [key,item]:roots){(void)key;if(item==value)return true;}return false;}
int __fastcall Retain(void* value,int,int,void*){roots[++root_id]=value;return root_id;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){Check(roots.erase(id)==1);}
GameEntity* __fastcall Create(EntityManager*,MapNpcDefinition*,NpcDataRow*,EntityCallback*,bool,void*){return E(*next_actor);}
GameWorld* __fastcall Owner(GameEntity* value,void*){return reinterpret_cast<Actor*>(value)->world;}
bool __fastcall Loaded(GameEntity*,void*){return true;}
unsigned __fastcall Id(GameEntity*,void*){return 1;}
void __fastcall Immediate(GameEntity*,bool,void*){}
void __fastcall Kill(GameWorld*,GameEntity* e,GameEntity*,void*){TrackedEntities().Disposed(TrackedEntities().Current(e));}
void* __fastcall GetComponent(GameEntity* value,void* type,void*) {
    Check(Kept(value) && Kept(type));
    for(std::size_t i=0;i<types.size();++i)if(type==&types[i])return &reinterpret_cast<Actor*>(value)->components[i];
    throw std::runtime_error("unexpected partner component type");
}
GameEntity* __fastcall GetLeader(void* value,void*){Check(value==&child.components[0] && Kept(value));return leader;}
void __fastcall Member(void* value,void* text,void* metadata) {
    Check(value==&child.components[0] && Kept(value) && Kept(text) && metadata==&info);
    ++member_calls;member_name=*static_cast<std::string*>(text);
}
// Independent fixture verifies offsets and x64 indirect-by-value placement.
void __fastcall Attach(void* value,const unsigned char* input,void* metadata) {
    Check(value==&child.components[1] && Kept(value) && metadata==&info);
    auto pointer=[&](std::size_t offset){void* result;std::memcpy(&result,input+offset,sizeof(result));return result;};
    Check(*static_cast<std::string*>(pointer(0))=="EffectScale" && pointer(8)==E(parent) &&
        *static_cast<std::string*>(pointer(16))=="PartnerPos01");
    Check(Kept(pointer(0)) && Kept(pointer(8)) && Kept(pointer(16)));
    Check(input[24]==1 && input[25]==0 && input[32]==2 && !input[33]);
    for(const auto offset:{26,27,28,29,30,31,34,35,36,37,38,39})Check(input[offset]==0);
    Check(pointer(40)==nullptr);++attach_calls;
    if(during_attach)during_attach();
    if(fault_attach)RaiseException(0xE0790001,0,0,nullptr);
}
void __fastcall Cancel(void* value,void* metadata) {
    Check(value==&child.components[1] && Kept(value) && metadata==&info);++cancel_calls;
    if(fault_cancel)RaiseException(0xE0790002,0,0,nullptr);
}
void __fastcall Anim(void* value,GameEntity* target,void* metadata) {
    Check(value==&child.components[2] && Kept(value) && metadata==&info);
    if(target)Check(Kept(target));++anim_calls;anim_target=target;
}
void __fastcall Idle(void* value,int logic,void* name,float start,float transition,float end,int extra,unsigned request,bool flag,void* metadata) {
    Check(value==&child.components[4] && Kept(value) && Kept(name) && metadata==&info);
    Check(logic==1 && !extra && !request && !flag);
    Check(*static_cast<std::string*>(name)==expected_idle.state && start==expected_idle.normalized_start &&
        transition==expected_idle.normalized_transition && end==expected_idle.normalized_end);
    ++idle_calls;if(fault_idle)RaiseException(0xE0790003,0,0,nullptr);
}
void __fastcall Visibility(void* value,GameEntity* target,VisibilityFollow flag,void* metadata) {
    Check(value==&child.components[3] && Kept(value) && metadata==&info);
    if(target)Check(Kept(target));++visible_calls;visibility_target=target;visibility_flag=flag;
}
void __fastcall SetVisibility(void* value,bool visible,const unsigned char* input,void* metadata) {
    Check(value==&child.components[3] && Kept(value) && metadata==&info);
    void* key=nullptr;std::memcpy(&key,input,sizeof(key));Check(key && Kept(key));
    for(int i=11;i<16;++i)Check(input[i]==0);
    const auto name=*static_cast<std::string*>(key);++visibility_sets;
    if(visible)hide_requests.erase(name);
    else hide_requests[name]={input[8]!=0,input[9]!=0,input[10]!=0};
}
struct Objects:ManagedObjects {
    std::vector<std::unique_ptr<std::string>> strings;
    ObjectResult NewStringUtf8(const char* text)override {
        strings.push_back(std::make_unique<std::string>(text));
        if(during_string)during_string();return {{true,0},strings.back().get()};
    }
    ObjectResult AllocateObject(const char*,const char*)override {throw std::runtime_error("no component or task allocation expected");}
    ObjectResult ManagedType(const char* ns,const char* name)override {
        const std::array<std::string,5> names{"OLABFALNLJM","TransformComponent","EAKGBLEMBLN","CharacterVisibleComponent","CFFPEAALCPH"};
        for(std::size_t i=0;i<names.size();++i)if(name==names[i]) {
            Check(std::string(ns)==(i==0 || i==2 || i==4?"":"RPG.GameCore"));return {{true,0},&types[i]};
        }
        throw std::runtime_error("unexpected managed type");
    }
};
MetadataResolver Resolver() {
    return MetadataResolver({
        {"RPG.GameCore","GameEntity",{
            {"GetComponent","RPG.GameCore.GameComponentBase",{"System.Type"},false,M(GetComponent)},
            {"get_OwnerWorldRef","RPG.GameCore.GameWorld",{},false,M(Owner)}}},
        {"","OLABFALNLJM",{
            {"set_MemberName","System.Void",{"System.String"},false,M(Member)},
            {"get_Leader","RPG.GameCore.GameEntity",{},false,M(GetLeader)}}},
        {"RPG.GameCore","TransformComponent",{
            {"AttachTo","System.Void",{"GDBHCNLLPAG"},false,M(Attach)},
            {"CancelSmoothFollow","System.Void",{},false,M(Cancel)}}},
        {"","EAKGBLEMBLN",{{"GBJGAIBJLBB","System.Void",{"RPG.GameCore.GameEntity"},false,M(Anim)}}},
        {"","CFFPEAALCPH",{{"ENBOEHDHCAL","System.Void",{"RPG.GameCore.AnimStateLogicType","System.String",
            "System.Single","System.Single","System.Single","RPG.GameCore.AnimStateLogicTypeExtra","System.UInt32","System.Boolean"},false,M(Idle)}}},
        {"RPG.GameCore","CharacterVisibleComponent",{{"SetFollowTargetVisibility","System.Void",
            {"RPG.GameCore.GameEntity","RPG.GameCore.CharacterVisiblityFollowFlag"},false,M(Visibility)},
            {"SetEntityVisibleParam","System.Void",{"System.Boolean","ANACIPLIMDE&"},false,M(SetVisibility)}}}
    });
}
}
int main() {
    using namespace hksr::api;
    Objects objects;EntityOwner owner,foreign_owner;auto resolver=Resolver();
    std::vector<DeclaredField> fields{{"RPG.GameCore","GameComponentBase","_OwnerRef","RPG.GameCore.GameEntity",16,4,true}};
    std::vector<DeclaredField> attach_fields{
        {"","GDBHCNLLPAG","ILDAAHNKMHL","System.String",16,6,true},
        {"","GDBHCNLLPAG","IGKONILBPEO","RPG.GameCore.GameEntity",24,6,true},
        {"","GDBHCNLLPAG","HFLDHJHJNCF","System.String",32,6,true},
        {"","GDBHCNLLPAG","BDNAGGIPGGN","System.Boolean",40,6,true},
        {"","GDBHCNLLPAG","FEDLPCHHOEG","System.Boolean",41,6,true},
        {"","GDBHCNLLPAG","KPNGMCPOMHJ","System.Int32",44,6,true},
        {"","GDBHCNLLPAG","KPAAEDABMDD","RPG.GameCore.CharacterVisiblityFollowFlag",48,6,true},
        {"","GDBHCNLLPAG","FCKCHPNEEKB","RPG.GameCore.GameEntity",56,6,true}};
    std::vector<DeclaredField> visibility_fields{
        {"","ANACIPLIMDE","KDOLHICFKLL","System.String",16,6,true},
        {"","ANACIPLIMDE","LPEFHLMHLPN","System.Boolean",24,6,true},
        {"","ANACIPLIMDE","ONMFCPIJGNP","System.Boolean",25,6,true},
        {"","ANACIPLIMDE","IEMGPGLKIIG","System.Boolean",26,6,true}};
    const auto methods=ResolvePartnerControl460(resolver,fields,attach_fields,visibility_fields);
    auto wrong_layout=attach_fields;wrong_layout[6].offset=44;
    bool rejected=false;try{ResolvePartnerControl460(resolver,fields,wrong_layout,visibility_fields);}catch(const std::exception&){rejected=true;}Check(rejected);
    wrong_layout=attach_fields;wrong_layout[0].flags|=0x10;
    rejected=false;try{ResolvePartnerControl460(resolver,fields,wrong_layout,visibility_fields);}catch(const std::exception&){rejected=true;}Check(rejected);
    auto wrong_visibility=visibility_fields;wrong_visibility[2].offset=27;
    rejected=false;try{ResolvePartnerControl460(resolver,fields,attach_fields,wrong_visibility);}catch(const std::exception&){rejected=true;}Check(rejected);
    const ReferenceMethods refs{M(Retain),M(Target),M(Free),{}, {}};
    EntityAccessMethods access;access.runtime_id=M(Id);access.owner_world=M(Owner);access.loaded=M(Loaded);
    access.kill_immediately=M(Immediate);access.make_die=M(Kill);
    NpcCreateInput input{world,reinterpret_cast<EntityManager*>(&manager_data),reinterpret_cast<MapNpcDefinition*>(&definition_data),reinterpret_cast<NpcDataRow*>(&row_data),false};
    WorldFrame frame;frame.world=world;frame.disposing={false,{true,0}};
    for(auto* actor:{&parent,&child,&other}){actor->world=world;for(auto& component:actor->components)component.owner=E(*actor);}
    NpcCreationSession a(owner,M(Create),access,refs),b(owner,M(Create),access,refs),c(foreign_owner,M(Create),access,refs);
    next_actor=&parent;a.Create(input);next_actor=&child;b.Create(input);next_actor=&other;c.Create(input);
    const auto p=owner.OwnedToken(E(parent)),q=owner.OwnedToken(E(child)),foreign=foreign_owner.OwnedToken(E(other));
    leader=p.entity;const auto creation_roots=roots.size();
    PartnerControlSession controls(owner,q,p,world,methods,refs,objects);
    Check(controls.SetMemberName("member").returned && member_name=="member");
    Check(controls.SetMemberName("").returned && member_name.empty());
    PartnerAttachment attach{p,"EffectScale","PartnerPos01",true,false,VisibilityFollow::Invisible};
    auto result=controls.Attach(attach);Check(result.returned && result.calls.size()==2 && result.references_released && attach_calls==1 && cancel_calls==1);
    Check(controls.FollowAnimation(p).returned && anim_target==p.entity);
    Check(controls.FollowAnimation().returned && !anim_target);
    Check(controls.FollowVisibility(p,VisibilityFollow::Invisible).returned && visibility_flag==VisibilityFollow::Invisible && visibility_target==p.entity);
    Check(controls.FollowVisibility(p,VisibilityFollow::None).returned && visibility_flag==VisibilityFollow::None && !visibility_target);
    Check(controls.FollowVisibility({},VisibilityFollow::Follow).returned && visibility_flag==VisibilityFollow::None);
    Check(controls.SetVisibility({"action",false,true,false,true}).returned);
    Check(hide_requests.at("action")[0] && !hide_requests.at("action")[1] && hide_requests.at("action")[2]);
    Check(controls.SetVisibility({"phase",false,false,true,false}).returned && hide_requests.size()==2);
    Check(controls.SetVisibility({"action",true}).returned && hide_requests.size()==1 && hide_requests.count("phase"));
    Check(controls.SetVisibility({"phase",true}).returned && hide_requests.empty());
    Check(!controls.SetVisibility({"",false}).returned && visibility_sets==4);
    Check(!controls.SetVisibility({std::string("a\0b",3),false}).returned && visibility_sets==4);
    expected_idle={"Skill11_Loop",0.25f,0.125f,1.5f};
    result=controls.PlayIdle(expected_idle);Check(result.returned && result.references_released && result.calls.size()==1 && idle_calls==1);
    Check(!controls.PlayIdle({""}).returned && !controls.PlayIdle({std::string("a\0b",3)}).returned);
    Check(!controls.PlayIdle({"state",0,-0.1f,1}).returned);
    Check(!controls.PlayIdle({"state",std::numeric_limits<float>::quiet_NaN()}).returned && idle_calls==1);
    child.components[4].owner=p.entity;Check(!controls.PlayIdle(expected_idle).returned && idle_calls==1);child.components[4].owner=q.entity;
    fault_idle=true;result=controls.PlayIdle(expected_idle);fault_idle=false;
    Check(!result.returned && result.calls.size()==1 && result.calls[0].exception==0xE0790003 && idle_calls==2);
    Check(!controls.FollowAnimation(foreign).returned && !controls.FollowAnimation(q).returned);
    parent.world=other_world;Check(!controls.SetMemberName("wrong-world").returned);parent.world=world;
    leader=foreign.entity;Check(!controls.FollowAnimation(p).returned);leader=p.entity;
    child.components[1].owner=p.entity;Check(!controls.Attach(attach).returned);child.components[1].owner=q.entity;
    fault_attach=true;result=controls.Attach(attach);fault_attach=false;
    Check(!result.returned && result.calls.size()==1 && result.calls[0].exception==0xE0790001 && cancel_calls==1);
    fault_cancel=true;result=controls.Attach(attach);fault_cancel=false;
    Check(!result.returned && result.calls.size()==2 && result.calls[0].returned && result.calls[1].exception==0xE0790002);
    during_attach=[&]{Check(!controls.Close());Check(!controls.SetMemberName("reentrant").returned);};
    Check(controls.Attach(attach).returned);during_attach={};
    const auto writes=member_calls;
    during_string=[&]{TrackedEntities().Disposed(q);};
    Check(!controls.SetMemberName("ended").returned && member_calls==writes);during_string={};
    Check(!controls.FollowAnimation(p).returned);
    Check(!controls.SetVisibility({"ended",false}).returned && visibility_sets==4);
    Check(!controls.PlayIdle(expected_idle).returned && idle_calls==2);
    Check(controls.Close() && controls.Close() && roots.size()==creation_roots);
    Check(!controls.SetMemberName("closed").returned);
    a.Remove();b.Remove();c.Remove();a.Service(frame);b.Service(frame);c.Service(frame);
    Check(a.Status().closed && b.Status().closed && c.Status().closed && roots.empty());
    std::cout<<"partner control native calls, lifetime and ABI passed\n";
}
