#include <hksr/presentation/hits/HitGeometry.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#include <hksr/presentation/PresentationBindings.hpp>
#include <hksr/animation/ActorAnimationSession.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <map>
#include <functional>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
using namespace hksr::api;
struct Logic {int pad=9;Position3 position{13,-15,9};} logic;
int info,anchor,root,position_calls=0,direction_calls=0;
Position3 seen_world;
void CheckAt(bool b,int line){if(!b)throw std::runtime_error("hit geometry test failed at "+std::to_string(line));}
#define Check(b) CheckAt((b),__LINE__)
bool Close(float a,float b){return std::abs(a-b)<1.e-5f;}
template<class T>Method M(T f){return {reinterpret_cast<void*>(f),&info};}
Position3 __fastcall Transform(void* object,Position3 offset,void* metadata) {
    Check(object==&logic && metadata==&info);return {offset.z,offset.y,-offset.x};
}
void __fastcall Position(void* object,Position3* out,void* metadata) {
    Check(object==&anchor && metadata==&info);++position_calls;*out={100,10,20};
}
void __fastcall Direction(void* object,Position3* in,Position3* out,void* metadata) {
    Check(object==&anchor && metadata==&info);++direction_calls;*out={in->z,in->y,-in->x};
}
void __fastcall Inverse(void* object,Position3* in,Position3* out,void* metadata) {
    Check(object==&root && metadata==&info);seen_world=*in;*out={in->x-100,in->y-10,in->z-20};
}
void* __fastcall Attach(void*,void*,bool create,void* metadata){Check(!create && metadata==&info);return &anchor;}
int entity,model_type,logic_type,animator,game_object,next_handle=0;
struct Model {int pad=7;bool flipped=true;} model;
std::map<int,void*> handles;
std::function<void()> on_euler;
int __fastcall Keep(void* object,int,int,void*){handles[++next_handle]=object;return next_handle;}
void* __fastcall Target(int handle,void*){return handles.at(handle);}
void __fastcall Free(int handle,void*){Check(handles.erase(handle)==1);}
void* __fastcall Component(void* object,void* type,void*) {
    Check(object==&entity);return type==&model_type?static_cast<void*>(&model):type==&logic_type?&logic:nullptr;
}
void* __fastcall Animator(void* object,void*){Check(object==&model);return &animator;}
bool __fastcall Changing(void* object,void*){Check(object==&model);return false;}
void* __fastcall GameObject(void* object,void*){Check(object==&entity);return &game_object;}
void* __fastcall RootTransform(void* object,void*){Check(object==&game_object);return &root;}
Position3 __fastcall Calculate(Position3 relative,float horizontal,float vertical,void*) {
    Check(horizontal==0 && vertical==0);return {-relative.x,0,-relative.z};
}
void __fastcall InverseDirection(void* object,Position3* in,Position3* out,void*){Check(object==&root);*out=*in;}
void __fastcall Rotation(void* object,Rotation4* out,void*){Check(object==&root);*out={};}
void __fastcall Look(Position3*,Position3*,Rotation4* out,void*){*out={};}
void __fastcall Euler(Rotation4*,Position3* out,void*){
    *out={0,1.57079632679f,0};if(on_euler){auto call=std::move(on_euler);on_euler={};call();}
}
struct Objects:ManagedObjects {
    ObjectResult ManagedType(const char* ns,const char* name)override {
        Check(std::string(ns)=="RPG.GameCore");
        const std::string type=name;Check(type=="CharacterModelComponent" || type=="TransformComponent");
        return {{true,0},type=="CharacterModelComponent"?&model_type:&logic_type};
    }
    ObjectResult NewStringUtf8(const char*)override{throw std::runtime_error("unexpected hit string allocation");}
    ObjectResult AllocateObject(const char*,const char*)override{throw std::runtime_error("unexpected hit object allocation");}
};
}
void TestHitGeometry() {
    MetadataResolver declarations({
        {"RPG.GameCore","TransformComponent",{{"TransformVector","RPG.MVector3",{"RPG.MVector3"},false,M(Transform)}}},
        {"RPG.GameCore","CharacterModelComponent",{{"GetAttachPoint","UnityEngine.Transform",{"System.String","System.Boolean"},false,M(Attach)}}},
        {"UnityEngine","Transform",{
            {"get_position_Injected","System.Void",{"UnityEngine.Vector3&"},false,M(Position)},
            {"TransformDirection_Injected","System.Void",{"UnityEngine.Vector3&","UnityEngine.Vector3&"},false,M(Direction)},
            {"InverseTransformPoint_Injected","System.Void",{"UnityEngine.Vector3&","UnityEngine.Vector3&"},false,M(Inverse)}}}});
    const std::vector<DeclaredField> fields{{"RPG.GameCore","TransformComponent","_WorldPos","RPG.MVector3",offsetof(Logic,position),0,true}};
    const auto methods=ResolveHitGeometryMethods(declarations,fields);Check(methods.binding_errors.empty());
    HitGeometry geometry(methods);
    HitShapeParameters input;input.width=.3f;input.height=1.5f;input.offset={1,2,3};
    const auto unanchored=geometry.Read(&logic,nullptr,input);
    Check(unanchored.read.returned && !unanchored.value.attached);
    Check(Close(unanchored.value.center.x,16) && Close(unanchored.value.center.y,-12.25f) && Close(unanchored.value.center.z,8));
    Check(Close(unanchored.value.vertical_extent,1.05f) && Close(unanchored.value.effect_radius,.3f));
    Check(position_calls==0 && direction_calls==0);
    auto attached=geometry.Read(nullptr,&anchor,input);
    Check(attached.read.returned && attached.value.attached);
    Check(Close(attached.value.center.x,103) && Close(attached.value.center.y,12) && Close(attached.value.center.z,19));
    input.kind=HitShapeKind::Box;input.width=2;input.height=4;
    attached=geometry.Read(nullptr,&anchor,input);
    Check(attached.read.returned && attached.value.vertical_extent==2 && attached.value.effect_radius==1);
    auto effect=geometry.EffectOffset(&root,attached.value,{103,30,29},1,0);
    Check(effect.read.returned && Close(seen_world.x,103) && Close(seen_world.z,20) && Close(seen_world.y,14));
    Check(Close(effect.value.x,3) && Close(effect.value.y,4) && Close(effect.value.z,0));
    effect=geometry.EffectOffset(&root,attached.value,{103,-30,29},1,90);
    Check(effect.read.returned && Close(seen_world.x,104) && Close(seen_world.z,19) && Close(seen_world.y,10));
    effect=geometry.EffectOffset(&root,attached.value,{93,11,19},.5f,0);
    Check(effect.read.returned && Close(seen_world.x,102) && Close(seen_world.z,19) && Close(seen_world.y,11.5f));
    // Coincident XZ uses the source's zero direction and atan2(0,0), not a
    // remembered previous attack or the actor's world rotation.
    effect=geometry.EffectOffset(&root,attached.value,{103,11,19},.5f,0);
    Check(effect.read.returned && Close(seen_world.x,103) && Close(seen_world.z,20));
    effect=geometry.EffectOffset(nullptr,{}, {},-1,0);
    Check(effect.read.returned && effect.value.x==0 && effect.value.y==0 && effect.value.z==0);
    Check(!geometry.EffectOffset(nullptr,attached.value,{},0,0).read.returned);
    input.kind=static_cast<HitShapeKind>(2);Check(!geometry.Read(&logic,nullptr,input).read.returned);
    input.kind=HitShapeKind::Capsule;input.width=-1;Check(!geometry.Read(&logic,nullptr,input).read.returned);
    auto missing=methods;missing.position={};input.width=1;
    Check(!HitGeometry(missing).Read(&logic,&anchor,input).read.returned); // no silent root fallback
    missing=ResolveHitGeometryMethods(declarations,{});
    Check(!HitGeometry(missing).Read(&logic,nullptr,input).read.returned);
    Check(HitGeometry(missing).Read(nullptr,&anchor,input).read.returned); // unused logical field isn't a gate
    const auto saved_position=logic.position;
    logic.position.x=std::numeric_limits<float>::infinity();Check(!geometry.Read(&logic,nullptr,input).read.returned);
    logic.position=saved_position;
    // Exercise the production session assembly, not only separate math fakes.
    PresentationBindings bindings;bindings.references={M(Keep),M(Target),M(Free),{}, {}};
    bindings.entity.component=M(Component);bindings.model.animator=M(Animator);bindings.model.changing=M(Changing);
    bindings.placement.game_object=M(GameObject);bindings.scene.transform=M(RootTransform);
    bindings.hit_geometry=methods;
    bindings.hit_direction.inverse_direction=M(InverseDirection);bindings.hit_direction.rotation=M(Rotation);
    bindings.hit_direction.look_rotation=M(Look);bindings.hit_direction.to_euler=M(Euler);
    bindings.hit_direction.calculate_world=M(Calculate);
    ResolveHitDirectionModel(bindings.hit_direction,{{"RPG.GameCore","CharacterModelComponent","_ModelFlip","System.Boolean",offsetof(Model,flipped),0,true}});
    Check(bindings.hit_direction.binding_errors.empty());
    auto* owned=reinterpret_cast<GameEntity*>(&entity);const auto token=TrackedEntities().Track(owned);
    Objects objects;ActorAnimationSession session(token,bindings);
    const auto angle=session.ReadHitAngle({13,100,19},input,"",0,0,objects);
    Check(angle.read.returned && angle.value==-90 && handles.size()==3);
    on_euler=[&]{TrackedEntities().Disposed(token);};
    Check(!session.ReadHitAngle({13,100,19},input,"",0,0,objects).read.returned);
    Check(session.Close() && handles.empty());
}
