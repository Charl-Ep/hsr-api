#include <hksr/world/WorldPlacement.hpp>
#include <hksr/native/Invocation.hpp>
#include <cmath>
#include <stdexcept>

namespace hksr::sdk_v1 {
WorldPlacement::WorldPlacement(WorldPlacementMethods methods):methods_(std::make_shared<const WorldPlacementMethods>(std::move(methods))){}
WorldPlacement::~WorldPlacement()=default;
WorldPlacement::WorldPlacement(const WorldPlacement&) noexcept=default;
WorldPlacement& WorldPlacement::operator=(const WorldPlacement&) noexcept=default;

WorldPlacementMethods ResolveWorldPlacementMethods(MethodResolver& source) {
    WorldPlacementMethods out;
    auto get=[&](const char* ns,const char* type,const char* name,const char* result,
        std::initializer_list<const char*> parameters=std::initializer_list<const char*>{}) {
        try {return source.Resolve(ns,type,name,result,parameters);}
        catch(const std::exception& e) {out.binding_errors.push_back(std::string(type)+"."+name+": "+e.what());return Method{};}
    };
    out.player=get("RPG.Client","AdventurePhase","get_CurrentLocalPlayer","RPG.GameCore.GameEntity");
    out.owner_world=get("RPG.GameCore","GameEntity","get_OwnerWorldRef","RPG.GameCore.GameWorld");
    out.game_object=get("RPG.GameCore","GameEntity","get_UnityGO","UnityEngine.GameObject");
    out.transform=get("UnityEngine","GameObject","get_transform","UnityEngine.Transform");
    out.position=get("UnityEngine","Transform","get_position_Injected","System.Void",{"UnityEngine.Vector3&"});
    out.rotation=get("UnityEngine","Transform","get_rotation_Injected","System.Void",{"UnityEngine.Quaternion&"});
    return out;
}
PlayerPlacement WorldPlacement::ReadPlayer(const WorldFrame& frame) const {
    PlayerPlacement out;
    if(!frame.world || !frame.adventure_phase.read.returned || !frame.adventure_phase.value ||
        !frame.disposing.read.returned || frame.disposing.value) {
        out.error="current exploration phase unavailable for player placement";return out;
    }
    auto object=[&](Method method,void* input,const char* label)->void* {
        void* value=nullptr;const auto result=native::Call(method,&value,input);
        if(!result.returned || !value) throw std::runtime_error(std::string(label)+" unavailable; exception="+std::to_string(result.exception));
        return value;
    };
    try {
        auto* player=object(methods_->player,frame.adventure_phase.value,"local player");
        if(object(methods_->owner_world,player,"player world")!=frame.world)
            throw std::runtime_error("local player belongs to another world");
        auto* go=object(methods_->game_object,player,"player GameObject");
        auto* transform=object(methods_->transform,go,"player Transform");
        // Injected by-reference APIs avoid platform-dependent struct returns.
        // Both reads are attempted independently; no player mutation occurs.
        out.position_read=native::CallVoid(methods_->position,transform,&out.position);
        out.rotation_read=native::CallVoid(methods_->rotation,transform,&out.rotation);
        const auto& p=out.position;const auto& q=out.rotation;
        out.ready=out.position_read.returned && out.rotation_read.returned &&
            std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
            std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w);
        if(!out.ready) out.error="player placement reads failed or returned non-finite values";
    } catch(const std::exception& e) {out.error=e.what();}
    return out;
}
PlayerPlacement WorldPlacement::ReadEntity(GameEntity* entity) const {
    PlayerPlacement out;void* go=nullptr;void* transform=nullptr;
    if(!entity || !native::Call(methods_->game_object,&go,entity).returned || !go ||
        !native::Call(methods_->transform,&transform,go).returned || !transform) {out.error="owned entity Transform unavailable";return out;}
    out.position_read=native::CallVoid(methods_->position,transform,&out.position);
    out.rotation_read=native::CallVoid(methods_->rotation,transform,&out.rotation);
    out.ready=out.position_read.returned && out.rotation_read.returned;
    if(!out.ready)out.error="owned entity placement read failed";
    return out;
}
ObjectResult WorldPlacement::Root(GameEntity* entity) const {
    ObjectResult result;if(entity)result.call=native::Call(methods_->game_object,&result.object,entity);return result;
}
GameEntity* WorldPlacement::PlayerEntity(const WorldFrame& frame) const {
    GameEntity* player=nullptr;GameWorld* world=nullptr;
    if(!frame.world || !frame.adventure_phase.read.returned || !frame.adventure_phase.value || !frame.disposing.read.returned || frame.disposing.value)return nullptr;
    if(!native::Call(methods_->player,&player,frame.adventure_phase.value).returned || !player)return nullptr;
    return native::Call(methods_->owner_world,&world,player).returned && world==frame.world?player:nullptr;
}
}
