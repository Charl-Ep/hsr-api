#include <hksr/animation/HitDirection.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
using namespace hksr::api;
int transform,info,rotation_calls,look_calls;
Position3 local{-3,5,-4},seen_direction,seen_forward,seen_up;
Rotation4 rotation{0,0,.70710678f,.70710678f};
float radians=0;
Position3 seen_relative;float seen_horizontal=0,seen_vertical=0;
void Check(bool value){if(!value)throw std::runtime_error("hit direction contract failed");}
template<class T>Method M(T fn){return {reinterpret_cast<void*>(fn),&info};}
void __fastcall Inverse(void* object,Position3* input,Position3* out,void* metadata) {
    Check(object==&transform && metadata==&info);seen_direction=*input;*out=local;
}
void __fastcall Rotation(void* object,Rotation4* out,void* metadata) {
    Check(object==&transform && metadata==&info);++rotation_calls;*out=rotation;
}
void __fastcall Look(Position3* forward,Position3* up,Rotation4* out,void* metadata) {
    Check(metadata==&info);++look_calls;seen_forward=*forward;seen_up=*up;*out={.1f,.2f,.3f,.4f};
}
void __fastcall Euler(Rotation4* in,Position3* out,void* metadata) {
    Check(metadata==&info && in->x==.1f && in->w==.4f);*out={0,radians,0};
}
Position3 __fastcall Calculate(Position3 direction,float horizontal,float vertical,void* metadata) {
    Check(metadata==&info);seen_relative=direction;seen_horizontal=horizontal;seen_vertical=vertical;
    return {-direction.x,-direction.y,-direction.z};
}
}
void TestHitDirection() {
    MetadataResolver declarations({
        {"UnityEngine","Transform",{
            {"InverseTransformDirection_Injected","System.Void",{"UnityEngine.Vector3&","UnityEngine.Vector3&"},false,M(Inverse)},
            {"get_rotation_Injected","System.Void",{"UnityEngine.Quaternion&"},false,M(Rotation)}}},
        {"UnityEngine","Quaternion",{
            {"LookRotation_Injected","System.Void",{"UnityEngine.Vector3&","UnityEngine.Vector3&","UnityEngine.Quaternion&"},true,M(Look)},
            {"Internal_ToEulerRad_Injected","System.Void",{"UnityEngine.Quaternion&","UnityEngine.Vector3&"},true,M(Euler)}}},
        {"RPG.GameCore","AbilityStatic",{
            {"_CalcHitDir","RPG.MVector3",{"RPG.MVector3","System.Single","System.Single"},true,M(Calculate)}}}});
    const auto methods=ResolveHitDirectionMethods(declarations);Check(methods.binding_errors.empty());
    HitDirection reader(methods);
    const auto world=reader.WorldDirection({13,9,24},{10,80,20},27,-15);
    Check(world.read.returned && std::abs(world.value.x+.6f)<1.e-6f && std::abs(world.value.z+.8f)<1.e-6f);
    Check(seen_relative.y==0 && seen_horizontal==27 && seen_vertical==-15);
    Check(reader.WorldDirection({10,9,20},{10,80,20}).read.returned && seen_relative.x==0 && seen_relative.z==0);
    Check(!reader.WorldDirection({}, {},std::numeric_limits<float>::infinity()).read.returned);
    auto no_calculation=methods;no_calculation.calculate_world={};
    Check(!HitDirection(no_calculation).WorldDirection({},{}).read.returned);
    auto angle=[&](float degrees,bool flip=false) {
        radians=degrees/57.295780181884766f;
        const auto result=reader.Read(&transform,{17,8,-9},flip);Check(result.read.returned);return result.value;
    };
    Check(angle(90)==90 && angle(90,true)==-90);
    Check(seen_direction.x==17 && seen_direction.y==8 && seen_direction.z==-9);
    Check(std::abs(seen_forward.x-.6f)<1.e-6f && seen_forward.y==0 && std::abs(seen_forward.z-.8f)<1.e-6f);
    Check(std::abs(seen_up.x+1)<1.e-6f && std::abs(seen_up.y)<1.e-6f && seen_up.z==0);
    Check(angle(45)==0 && angle(-45)==0 && angle(135)==180 && angle(-135)==-180);
    Check(angle(46)==90 && angle(-46)==-90 && angle(270)==-90);
    Check(angle(-.001f)==0 && angle(359.999f)==0);
    const auto rotations=rotation_calls,looks=look_calls;local={0,9,0};
    Check(angle(90)==0 && rotation_calls==rotations && look_calls==looks);
    Check(!reader.Read(nullptr,{1,0,0}).read.returned);
    Check(!reader.Read(&transform,{std::numeric_limits<float>::infinity(),0,0}).read.returned);
    local={1,0,0};auto missing=methods;missing.look_rotation={};
    Check(!HitDirection(missing).Read(&transform,{1,0,0}).read.returned);
    radians=std::numeric_limits<float>::quiet_NaN();Check(!reader.Read(&transform,{1,0,0}).read.returned);
}
