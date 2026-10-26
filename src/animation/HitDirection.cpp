#include <hksr/animation/HitDirection.hpp>
#include <hksr/native/Invocation.hpp>
#include <cmath>
#include <utility>

namespace hksr::sdk_v1 {
namespace {
bool Finite(Position3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
// Mathf.Round's tie-to-even behavior, independent of the process rounding mode.
float RoundEven(float value) {
    const auto lower=std::floor(value),fraction=value-lower;
    if(fraction<.5f)return lower;
    if(fraction>.5f)return lower+1;
    return std::fmod(std::abs(lower),2.f)==0?lower:lower+1;
}
}
HitDirection::HitDirection(HitDirectionMethods methods):methods_(std::move(methods)){}
Observed<float> HitDirection::Read(void* transform,Position3 direction,bool mirrored) const {
    Observed<float> out;
    if(!transform || !Finite(direction))return out;
    Position3 local;
    out.read=native::CallVoid(methods_.inverse_direction,transform,&direction,&local);
    if(!out.read.returned || !Finite(local)){out.read.returned=false;return out;}
    local.y=0;
    const auto magnitude=std::sqrt(local.x*local.x+local.z*local.z);
    if(!std::isfinite(magnitude)){out.read.returned=false;return out;}
    if(magnitude<=1.e-5f){out.value=0;return out;}
    local.x=-local.x/magnitude;local.z=-local.z/magnitude;
    Rotation4 rotation;
    out.read=native::CallVoid(methods_.rotation,transform,&rotation);
    if(!out.read.returned || !Finite({rotation.x,rotation.y,rotation.z}) || !std::isfinite(rotation.w)) {
        out.read.returned=false;return out;
    }
    // The source uses the Transform rotation applied to Vector3.up, not a
    // fixed up vector and not FromToRotation. Keep Unity's LookRotation/Euler.
    Position3 up{2*(rotation.x*rotation.y-rotation.w*rotation.z),
        1-2*(rotation.x*rotation.x+rotation.z*rotation.z),
        2*(rotation.y*rotation.z+rotation.w*rotation.x)};
    if(!Finite(up)){out.read.returned=false;return out;}
    Rotation4 looking;Position3 euler;
    out.read=native::CallVoid(methods_.look_rotation,&local,&up,&looking);
    if(!out.read.returned || !Finite({looking.x,looking.y,looking.z}) || !std::isfinite(looking.w)) {
        out.read.returned=false;return out;
    }
    out.read=native::CallVoid(methods_.to_euler,&looking,&euler);
    if(!out.read.returned || !Finite(euler)){out.read.returned=false;return out;}
    float degrees=euler.y*57.295780181884766f;
    if(degrees<-.005729577969759703f)degrees+=360;
    else if(degrees>359.9942626953125f)degrees-=360;
    if(degrees>180)degrees-=360;
    out.value=RoundEven(degrees/90.f)*90.f;
    if(mirrored)out.value=-out.value;
    return out;
}
HitDirectionMethods ResolveHitDirectionMethods(MethodResolver& source) {
    HitDirectionMethods out;
    auto get=[&](const char* type,const char* name,std::initializer_list<const char*> parameters,MethodKind kind) {
        try{return source.Resolve("UnityEngine",type,name,"System.Void",parameters,kind);}
        catch(const std::exception& e){out.binding_errors.push_back(std::string(type)+"."+name+": "+e.what());return Method{};}
    };
    out.inverse_direction=get("Transform","InverseTransformDirection_Injected",{"UnityEngine.Vector3&","UnityEngine.Vector3&"},MethodKind::Instance);
    out.rotation=get("Transform","get_rotation_Injected",{"UnityEngine.Quaternion&"},MethodKind::Instance);
    out.look_rotation=get("Quaternion","LookRotation_Injected",{"UnityEngine.Vector3&","UnityEngine.Vector3&","UnityEngine.Quaternion&"},MethodKind::Static);
    out.to_euler=get("Quaternion","Internal_ToEulerRad_Injected",{"UnityEngine.Quaternion&","UnityEngine.Vector3&"},MethodKind::Static);
    return out;
}
}
