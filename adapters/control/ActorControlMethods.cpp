#include <hksr/control/ActorControl.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
ActorControlMethods ResolveActorControlMethods(MethodResolver& source) {
    ActorControlMethods out;
    auto get=[&](const char* ns,const char* type,const char* name,const char* result,std::initializer_list<const char*> params=std::initializer_list<const char*>{}) {
        try{return source.Resolve(ns,type,name,result,params);}
        catch(const std::exception& e){out.binding_errors.push_back(std::string(type)+"."+name+": "+e.what());return Method{};}
    };
    out.flush=get("RPG.GameCore","AdventureCharacterController","FlushMovementThreadData","System.Void");
    out.place=get("RPG.GameCore","AdventureCharacterController","SetCharacterPositionAndRotation","System.Void",{"UnityEngine.Vector3","UnityEngine.Quaternion"});
    out.mono=get("RPG.GameCore","AdventureCharacterController","get_MonoAvatar","RPG.Client.CharacterMonoAvatar");
    out.look_enabled=get("","PKBOEALAKCM","CIMKMBDNGIK","System.Boolean");
    out.set_look_enabled=get("","PKBOEALAKCM","IDCGHHPJLGF","System.Void",{"System.Boolean"});
    out.head=get("RPG.Client","CharacterMonoAvatar","get_Head","UnityEngine.Transform");
    out.look=get("RPG.Client","CharacterMonoAvatar","get_LookAtIK","RPG.Client.LookAtIK");
    out.follow=get("RPG.Client","CharacterMonoAvatar","HeadLookAt","System.Void",{"UnityEngine.Transform","System.Boolean","System.Boolean","RPG.GameCore.CharacterHeadConstraint"});
    out.stop=get("RPG.Client","CharacterMonoAvatar","HeadStopLookAt","System.Void",{"System.Boolean"});
    out.point=get("UnityEngine","Transform","set_position_Injected","System.Void",{"UnityEngine.Vector3&"});
    out.model_attach_point=get("RPG.GameCore","CharacterModelComponent","GetAttachPoint","UnityEngine.Transform",{"System.String","System.Boolean"});
    out.forbid_ground=get("RPG.GameCore","AdventureCharacterController","set_ForbidStandOnGround","System.Void",{"System.Boolean"});
    out.get_forbid_ground=get("RPG.GameCore","AdventureCharacterController","get_ForbidStandOnGround","System.Boolean");
    out.navigation_only=get("RPG.GameCore","AdventureCharacterController","set_UseNavigationMotionOnly","System.Void",{"System.Boolean"});
    out.get_navigation_only=get("RPG.GameCore","AdventureCharacterController","get_UseNavigationMotionOnly","System.Boolean");
    out.navigation_ground=get("RPG.GameCore","AdventureCharacterController","set_UseNavigationToGround","System.Void",{"System.Boolean"});
    out.get_navigation_ground=get("RPG.GameCore","AdventureCharacterController","get_UseNavigationToGround","System.Boolean");
    out.ik_head=get("RPG.Client","LookAtIK","get_Head","UnityEngine.Transform");
    out.ik_follow=get("RPG.Client","LookAtIK","HeadLookAt","System.Void",{"UnityEngine.Transform","System.Boolean","System.Boolean","RPG.GameCore.CharacterHeadConstraint"});
    out.ik_stop=get("RPG.Client","LookAtIK","HeadStopLookAt","System.Void",{"System.Boolean"});
    out.ik_active=get("RPG.Client","LookAtIK","get_IsActive","System.Boolean");
    out.behaviour_enabled=get("UnityEngine","Behaviour","get_enabled","System.Boolean");
    out.set_behaviour_enabled=get("UnityEngine","Behaviour","set_enabled","System.Void",{"System.Boolean"});
    return out;
}
}
