#include "../../src/entity/PartnerEventCleanup.hpp"
#include <hksr/entity/EntityCreationSession.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#include <Windows.h>
#include <array>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
using namespace hksr::api;
using namespace hksr::sdk_v1::detail;
void Check(bool value){if(!value)throw std::runtime_error("partner event cleanup test failed");}
struct Group {void* header[2]{};GameEntity* owner=nullptr;};
struct Actor {GameWorld* world=nullptr;Group group;bool has_group=true;};
struct Callback {void* target=nullptr;void* method=nullptr;};
struct Events {std::vector<Callback> listeners;};
struct World {unsigned char prefix[88]{};Events* events=nullptr;};
Events events;World world_data;Actor child,other,foreign,anchor;
auto* world=reinterpret_cast<GameWorld*>(&world_data);
GameEntity* E(Actor& a){return reinterpret_cast<GameEntity*>(&a);}
int info,callback_info,group_type,definition,row,manager;
Actor* next=nullptr;
unsigned removals=0,constructions=0,root_id=0;
std::map<unsigned,void*> roots;
std::function<void()> inside_factory,inside_ctor;
bool fail_unregister=false,fail_free=false;
template<class F>Method M(F fn){return {reinterpret_cast<void*>(fn),&info};}
bool Kept(void* object){for(const auto& [id,value]:roots){(void)id;if(value==object)return true;}return false;}
unsigned __fastcall Retain(void* object,int,int,void*){roots[++root_id]=object;return root_id;}
void* __fastcall Target(unsigned id,void*){return roots.at(id);}
void __fastcall Free(unsigned id,void*){if(fail_free)RaiseException(0xE0890011,0,0,nullptr);Check(roots.erase(id)==1);}
GameEntity* __fastcall Create(EntityManager*,MapNpcDefinition*,NpcDataRow*,EntityCallback*,bool,void*) {
    EntityCreationScope::FactoryCompleted(world,E(*next));
    if(inside_factory)inside_factory();
    return E(*next);
}
GameWorld* __fastcall Owner(GameEntity* a,void*){return reinterpret_cast<Actor*>(a)->world;}
void* __fastcall Component(GameEntity* a,void* type,void*) {
    Check(type==&group_type && Kept(type) && Kept(a));auto* actor=reinterpret_cast<Actor*>(a);
    return actor->has_group?&actor->group:nullptr;
}
void __fastcall CallbackBody(void*,void*,void*){throw std::runtime_error("callback body must never be executed");}
void __fastcall Ctor(Callback* value,void* target,void* method,void* metadata) {
    Check(Kept(value) && Kept(target) && method==&callback_info && metadata==&info);
    value->target=target;value->method=method;++constructions;
    if(inside_ctor)inside_ctor();
}
void __fastcall Unregister(Events* source,int event,unsigned filter,Callback* callback,void* metadata) {
    Check(source==&events && Kept(source) && Kept(callback) && event==66 && filter==1 && metadata==&info);
    ++removals;
    for(auto it=source->listeners.begin();it!=source->listeners.end();++it)
        if(it->target==callback->target && it->method==callback->method){source->listeners.erase(it);break;}
    if(fail_unregister)RaiseException(0xE0890010,0,0,nullptr);
}
struct Objects:ManagedObjects {
    std::vector<std::unique_ptr<Callback>> callbacks;
    ObjectResult NewStringUtf8(const char*)override{throw std::runtime_error("no string/name lookup needed");}
    ObjectResult ManagedType(const char* ns,const char* name)override {
        Check(std::string(ns).empty() && std::string(name)=="OLABFALNLJM");return {{true,0},&group_type};
    }
    ObjectResult AllocateObject(const char* ns,const char* name)override {
        Check(std::string(ns)=="RPG.GameCore" && std::string(name)=="OnEventCallBack");
        callbacks.push_back(std::make_unique<Callback>());return {{true,0},callbacks.back().get()};
    }
};
MetadataResolver Resolver() {
    return MetadataResolver({
        {"RPG.GameCore","GameEntity",{{"GetComponent","RPG.GameCore.GameComponentBase",{"System.Type"},false,M(Component)},
            {"get_OwnerWorldRef","RPG.GameCore.GameWorld",{},false,M(Owner)}}},
        {"RPG.GameCore","OnEventCallBack",{{".ctor","System.Void",{"System.Object","System.IntPtr"},false,M(Ctor)}}},
        {"","OLABFALNLJM",{{"HEMJBALNMKB","System.Void",{"EKFMIDBJDIA"},false,{reinterpret_cast<void*>(&CallbackBody),&callback_info}}}},
        {"RPG.GameCore","EventManager",{{"UnRegisterEventListener","System.Void",
            {"RPG.GameCore.EventType","System.UInt32","RPG.GameCore.OnEventCallBack"},false,M(Unregister)}}}
    });
}
}
int main() {
    using namespace hksr::api;using namespace hksr::sdk_v1::detail;
    Objects objects;EntityOwner owner,foreign_owner;auto resolver=Resolver();
    const ReferenceMethods refs{M(Retain),M(Target),M(Free),{}, {}};
    std::vector<DeclaredField> component_fields{{"RPG.GameCore","GameComponentBase","_OwnerRef","RPG.GameCore.GameEntity",16,4,true}};
    std::vector<DeclaredField> world_fields{{"RPG.GameCore","GameWorld","_EventManager","RPG.GameCore.EventManager",88,1,true}};
    const auto methods=ResolvePartnerEventCleanup460(resolver,component_fields,world_fields);
    auto wrong=world_fields;wrong[0].flags|=0x10;
    bool rejected=false;try{ResolvePartnerEventCleanup460(resolver,component_fields,wrong);}catch(const std::exception&){rejected=true;}Check(rejected);
    world_data.events=&events;
    for(auto* a:{&child,&other,&foreign,&anchor}){a->world=world;a->group.owner=E(*a);}anchor.has_group=false;
    EntityAccessMethods access;access.owner_world=M(Owner);
    NpcCreateInput input{world,reinterpret_cast<EntityManager*>(&manager),reinterpret_cast<MapNpcDefinition*>(&definition),reinterpret_cast<NpcDataRow*>(&row),false};
    auto create=[&](Actor& actor,const EntityOwner& client) {
        auto session=std::make_unique<EntityCreationSession>(client,M(Create),access,refs);
        next=&actor;session->Create(input);Check(session->Status().create_returned);return session;
    };
    PartnerEventCleanup cleanup(owner,methods,refs,objects);
    // Owned inside a factory, before adoption into the tracked registry.
    inside_factory=[&] {
        events.listeners={{&child.group,&callback_info},{&other.group,&callback_info},{&child.group,&info}};
        const auto token=owner.OwnedToken(E(child));Check(token.entity && !TrackedEntities().Current(E(child)).entity);
        Check(cleanup.Remove({token,world}).returned);
    };
    auto a=create(child,owner);inside_factory={};auto b=create(other,owner),c=create(foreign,foreign_owner),d=create(anchor,owner);
    const auto child_token=owner.OwnedToken(E(child)),other_token=owner.OwnedToken(E(other));
    Check(removals==1 && constructions==1 && events.listeners.size()==2);
    Check(events.listeners[0].target==&other.group && events.listeners[1].method==&info);
    Check(cleanup.Remove({child_token,world}).returned && removals==1);
    Check(!cleanup.Remove({foreign_owner.OwnedToken(E(foreign)),world}).returned && removals==1);
    Check(cleanup.Remove({owner.OwnedToken(E(anchor)),world}).returned && constructions==1);
    Check(cleanup.Close());
    // Repeated SDK cleanup after an official unsubscribe is harmless; this
    // is value equality, not equal allocation addresses or whole-key clearing.
    PartnerEventCleanup absent(owner,methods,refs,objects);
    Check(absent.Remove({child_token,world}).returned && events.listeners.size()==2);
    Check(absent.Close());
    PartnerEventCleanup retirement(owner,methods,refs,objects);
    auto id=TrackedEntities().BeforeDispose(other_token,[&](auto token){Check(retirement.Remove({token,world}).returned);});
    Check(TrackedEntities().DisposalStarting(other_token).invoked==1 && events.listeners.size()==1);
    TrackedEntities().Disposed(other_token);TrackedEntities().CancelBeforeDispose(id);Check(retirement.Close());
    // A partial native mutation is recorded once, never retried after an SEH.
    PartnerEventCleanup partial(owner,methods,refs,objects);fail_unregister=true;
    const auto result=partial.Remove({child_token,world});const auto count=removals;
    Check(!result.returned && result.unregister_attempted && result.call.exception==0xE0890010);
    Check(!partial.Remove({child_token,world}).returned && removals==count);fail_unregister=false;Check(partial.Close());
    // Unknown GC decrement is not repeated. It does not conceal the successful
    // unregister result or turn disposal into a retry loop.
    PartnerEventCleanup unresolved(owner,methods,refs,objects);
    Check(unresolved.Remove({child_token,world}).returned);fail_free=true;Check(!unresolved.Close());
    const auto pinned=roots.size();fail_free=false;Check(!unresolved.Close() && roots.size()==pinned);
    for(auto* session:{a.get(),b.get(),c.get(),d.get()}) {
        for(const auto& entity:session->Status().entities)TrackedEntities().Disposed(entity.token);
        session->Service({});Check(session->Status().closed);
    }
    std::cout<<"Partner exact callback retirement tests passed.\n";
}
