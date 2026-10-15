#include <hksr/world/WorldMap.hpp>
#include <hksr/world/WorldPlacement.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#include <Windows.h>
#include <iostream>
#include <stdexcept>

namespace {
using namespace hksr::api;
int world,phase,group,map,info,calls;
bool fail_floor=false,fail_runtime=false,missing_group=false;
void Check(bool value){if(!value)throw std::runtime_error("world map access test failed");}
void Self(void* object,void* expected,void* metadata){Check(object==expected && metadata==&info);++calls;}
void* __fastcall Group(void* object,void* metadata){Self(object,&phase,metadata);return missing_group?nullptr:&group;}
void* __fastcall Map(void* object,void* metadata){Self(object,&group,metadata);return &map;}
bool __fastcall Pending(void* object,void* metadata){Self(object,&phase,metadata);return true;}
std::uint32_t __fastcall Plane(void* object,void* metadata){Self(object,&map,metadata);return 100;}
std::uint32_t __fastcall Floor(void* object,void* metadata){Self(object,&map,metadata);if(fail_floor)RaiseException(0xE0630001,0,0,nullptr);return 200;}
std::uint32_t __fastcall Zero(void* object,void* metadata){Self(object,&map,metadata);return 0;}
std::uint64_t __fastcall Runtime(void* object,void* metadata){Self(object,&map,metadata);if(fail_runtime)RaiseException(0xE0630002,0,0,nullptr);return 0x100000001ull;}
template<class F> Method M(F function){return {reinterpret_cast<void*>(function),&info};}
int player,go,transform,rotation_reads=0;
bool wrong_world=false,fail_position=false;
void* __fastcall Player(void* object,void* metadata){Self(object,&phase,metadata);return &player;}
void* __fastcall Owner(void* object,void* metadata){Self(object,&player,metadata);return wrong_world?&map:&world;}
void* __fastcall Go(void* object,void* metadata){Self(object,&player,metadata);return &go;}
void* __fastcall Transform(void* object,void* metadata){Self(object,&go,metadata);return &transform;}
void __fastcall Position(void* object,Position3* value,void* metadata){Self(object,&transform,metadata);if(fail_position)RaiseException(0xE0630003,0,0,nullptr);*value={10,20,30};}
void __fastcall Rotation(void* object,Rotation4* value,void* metadata){Self(object,&transform,metadata);++rotation_reads;*value={0,1,0,0};}
void TestPlacement() {
    MetadataResolver source({
        {"RPG.Client","AdventurePhase",{{"get_CurrentLocalPlayer","RPG.GameCore.GameEntity",{},false,M(Player)}}},
        {"RPG.GameCore","GameEntity",{{"get_OwnerWorldRef","RPG.GameCore.GameWorld",{},false,M(Owner)},
            {"get_UnityGO","UnityEngine.GameObject",{},false,M(Go)}}},
        {"UnityEngine","GameObject",{{"get_transform","UnityEngine.Transform",{},false,M(Transform)}}},
        {"UnityEngine","Transform",{{"get_position_Injected","System.Void",{"UnityEngine.Vector3&"},false,M(Position)},
            {"get_rotation_Injected","System.Void",{"UnityEngine.Quaternion&"},false,M(Rotation)}}}});
    const auto methods=ResolveWorldPlacementMethods(source);Check(methods.binding_errors.empty());
    WorldFrame frame;frame.world=reinterpret_cast<GameWorld*>(&world);frame.adventure_phase={&phase,{true,0}};frame.disposing={false,{true,0}};
    WorldPlacement access(methods);auto result=access.ReadPlayer(frame);
    Check(result.ready && result.position.x==10 && result.position.y==20 && result.position.z==30 && result.rotation.y==1);
    fail_position=true;result=access.ReadPlayer(frame);fail_position=false;
    Check(!result.ready && result.position_read.exception==0xE0630003 && result.rotation_read.returned && rotation_reads==2);
    wrong_world=true;result=access.ReadPlayer(frame);wrong_world=false;
    Check(!result.ready && !result.position_read.returned && rotation_reads==2);
    frame.disposing.value=true;calls=0;Check(!access.ReadPlayer(frame).ready && calls==0);
    MetadataResolver empty({});Check(ResolveWorldPlacementMethods(empty).binding_errors.size()==6);
}
}
void TestWorldMap(){
    MetadataResolver source({
        {"RPG.Client","AdventurePhase",{
            {"get_RuntimeGroupManager","RPG.Client.RuntimeGroupManager",{},false,M(Group)},
            {"get_PendingSwitchScene","System.Boolean",{},false,M(Pending)}}},
        {"RPG.Client","RuntimeGroupManager",{{"GetMapDef","RPG.Client.MapDef",{},false,M(Map)}}},
        {"RPG.Client","MapDef",{
            {"get_PlaneID","System.UInt32",{},false,M(Plane)},
            {"get_FloorID","System.UInt32",{},false,M(Floor)},
            {"get_DimensionID","System.UInt32",{},false,M(Zero)},
            {"get_ContentID","System.UInt32",{},false,M(Zero)},
            {"get_StoryLineID","System.UInt32",{},false,M(Zero)},
            {"get_CurrentGameMode","RPG.GameCore.GameModeType",{},false,M(Zero)},
            {"get_MapRuntimeID","System.UInt64",{},false,M(Runtime)}}}});
    const auto methods=ResolveWorldMapMethods(source);Check(methods.binding_errors.empty());
    WorldFrame frame;frame.world=reinterpret_cast<GameWorld*>(&world);
    frame.adventure_phase={&phase,{true,0}};frame.disposing={false,{true,0}};
    WorldMap reader(methods);auto result=reader.Read(frame);
    Check(result.group_manager.value==&group && result.map.value==&map && result.pending_switch.value);
    Check(result.identity[0].value==100 && result.identity[1].value==200 && result.identity[2].read.returned && !result.identity[2].value);
    Check(result.runtime_id.value==0x100000001ull && calls==10);
    fail_floor=fail_runtime=true;result=reader.Read(frame);
    Check(!result.identity[1].read.returned && result.identity[1].read.exception==0xE0630001);
    Check(result.identity[5].read.returned && !result.runtime_id.read.returned);
    fail_floor=fail_runtime=false;
    missing_group=true;calls=0;result=reader.Read(frame);
    Check(result.group_manager.read.returned && !result.group_manager.value && !result.map.read.returned && calls==2);
    missing_group=false;frame.disposing.value=true;calls=0;reader.Read(frame);Check(!calls);
    frame.disposing.value=false;frame.adventure_phase.read.returned=false;reader.Read(frame);Check(!calls);
    MetadataResolver empty({});const auto absent=ResolveWorldMapMethods(empty);Check(absent.binding_errors.size()==10);
    frame.adventure_phase.read.returned=true;Check(!WorldMap(absent).Read(frame).map.read.returned);
    std::cout<<"World map access passed: named phase/group chain, independent values, no entity scan or readiness inference.\n";
    TestPlacement();
    std::cout<<"Player placement passed: official named chain, reference outputs, world identity, independent errors; no player writes.\n";
}
