#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#include <Windows.h>
#include <iostream>
#include <stdexcept>

namespace {
using namespace hksr::api;
int world_storage, entity_storage, anchor_storage, method_info;
volatile unsigned create_calls=0, dispose_calls=0;
bool fail_dispose=false;
std::function<void()> inside_dispose;
auto* world=reinterpret_cast<GameWorld*>(&world_storage);
auto* entity=reinterpret_cast<GameEntity*>(&entity_storage);
auto* anchor=reinterpret_cast<GameEntity*>(&anchor_storage);
void Check(bool value){if(!value)throw std::runtime_error("native lifecycle hook test failed");}
__declspec(noinline) GameEntity* __fastcall NativeCreateNew(GameWorld* w,std::uint32_t id,int team,int type,void* metadata){
    Check(w==world && team==4 && metadata==&method_info);
    Check((id==77 && type==4) || (id==78 && type==18));
    return id==77?entity:anchor;
}
__declspec(noinline) GameEntity* __fastcall NativeCreate(GameWorld* w,int team,int type,void* metadata){
    Check(w==world && team==4 && type==4 && metadata==&method_info);
    ++create_calls;
    return NativeCreateNew(w,77,team,type,metadata);
}
__declspec(noinline) void __fastcall NativeDispose(GameEntity* e,void* metadata){
    Check(e==entity && metadata==&method_info);
    ++dispose_calls;
    if(inside_dispose)inside_dispose();
    if(fail_dispose)RaiseException(0xE0434343,0,0,nullptr);
}
bool TryDispose(){
    __try{NativeDispose(entity,&method_info);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
void TestLifecycleHooks(){
    MetadataResolver resolver({
        {"RPG.GameCore","GameWorld",{
            {"CreateEmptyEntity","RPG.GameCore.GameEntity",{"RPG.GameCore.TeamType","RPG.GameCore.EntityType"},false,{reinterpret_cast<void*>(&NativeCreate),&method_info}},
            {"_CreateNewEntityImpl","RPG.GameCore.GameEntity",{"System.UInt32","RPG.GameCore.TeamType","RPG.GameCore.EntityType"},false,{reinterpret_cast<void*>(&NativeCreateNew),&method_info}}}},
        {"RPG.GameCore","GameEntity",{{"Dispose","System.Void",{},false,{reinterpret_cast<void*>(&NativeDispose),&method_info}}}}
    });
    InstallEntityLifecycleHooks(resolver);
    auto& tracked=TrackedEntities();
    const auto token=tracked.Track(entity);
    unsigned observed=0;
    tracked.BeforeDispose(token,[&](auto current){Check(current.generation==token.generation && dispose_calls==0);++observed;});
    tracked.BeforeDispose(token,[](auto){throw std::runtime_error("observer failure must not suppress original");});
    tracked.BeforeDispose(token,[](auto){RaiseException(0xE0670003,0,0,nullptr);});
    inside_dispose=[&]{Check(observed==1 && tracked.IsRetiring(token) && !tracked.IsDisposed(token));};
    {
        unsigned births=0;
        EntityCreationScope receipt(world,nullptr,[&](const auto&){++births;});
        Check(NativeCreate(world,4,4,&method_info)==entity);
        Check(create_calls==1 && receipt.Receipt().Unique()==entity && births==1);
        Check(NativeCreateNew(world,78,4,18,&method_info)==anchor);
        Check(receipt.Receipt().entities.size()==2 && births==2);
    }
    fail_dispose=true;Check(!TryDispose());
    Check(dispose_calls==1 && !tracked.IsDisposed(token));
    fail_dispose=false;Check(TryDispose());
    Check(dispose_calls==2 && tracked.IsDisposed(token));
    Check(observed==1);inside_dispose={};
    tracked.Forget(token);
    RemoveEntityLifecycleHooksAtQuiescentPoint();
    {
        EntityCreationScope receipt(world);
        Check(NativeCreate(world,4,4,&method_info)==entity);
        Check(NativeCreateNew(world,78,4,18,&method_info)==anchor);
        Check(create_calls==2 && receipt.Receipt().entities.empty());
    }
    // A later installation is explicit and works without duplicate hooks.
    InstallEntityLifecycleHooks(resolver);
    RemoveEntityLifecycleHooksAtQuiescentPoint();
    MetadataResolver broken({
        {"RPG.GameCore","GameWorld",{
            {"CreateEmptyEntity","RPG.GameCore.GameEntity",{"RPG.GameCore.TeamType","RPG.GameCore.EntityType"},false,{reinterpret_cast<void*>(&NativeCreate),&method_info}},
            {"_CreateNewEntityImpl","RPG.GameCore.GameEntity",{"System.UInt32","RPG.GameCore.TeamType","RPG.GameCore.EntityType"},false,{reinterpret_cast<void*>(&NativeCreateNew),&method_info}}}},
        {"RPG.GameCore","GameEntity",{{"Dispose","System.Void",{},false,{reinterpret_cast<void*>(1),&method_info}}}}
    });
    bool failed=false;
    try{InstallEntityLifecycleHooks(broken);}catch(const std::exception&){failed=true;}
    Check(failed);
    {
        EntityCreationScope receipt(world);
        NativeCreate(world,4,4,&method_info);
        Check(receipt.Receipt().entities.empty());
    }
    // A failed second hook must not strand the first hook or prevent retry.
    InstallEntityLifecycleHooks(resolver);
    RemoveEntityLifecycleHooksAtQuiescentPoint();
    std::cout<<"Actual native hook install/forward/remove tests passed in test process only.\n";
}
