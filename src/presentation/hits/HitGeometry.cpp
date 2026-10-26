#include <hksr/presentation/hits/HitGeometry.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <algorithm>
#include <cmath>
#include <utility>

namespace hksr::sdk_v1 {
namespace {
bool Finite(Position3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
bool Nonnegative(float x){return std::isfinite(x)&&x>=0;}
}
HitGeometry::HitGeometry(HitGeometryMethods methods):methods_(std::move(methods)){}
ObjectResult HitGeometry::Anchor(void* model,void* name) const {
    ObjectResult out;
    if(model && name)out.call=native::Call(methods_.attach_point,&out.object,model,name,false);
    return out;
}
Observed<Position3> HitGeometry::LogicalPosition(void* transform) const {
    Observed<Position3> out;
    if(!transform || !methods_.logical_position_available)return out;
    out.read.returned=native::CopyMemoryChecked(&out.value,
        static_cast<const unsigned char*>(transform)+methods_.logical_position_offset,sizeof(out.value)) && Finite(out.value);
    return out;
}
Observed<HitShapeSnapshot> HitGeometry::Read(void* transform,void* anchor,const HitShapeParameters& parameters) const {
    Observed<HitShapeSnapshot> out;
    if((parameters.kind!=HitShapeKind::Capsule && parameters.kind!=HitShapeKind::Box) ||
        !Nonnegative(parameters.width) || !Nonnegative(parameters.height) || !Finite(parameters.offset))return out;
    Position3 position,offset;
    if(anchor) {
        out.read=native::CallVoid(methods_.position,anchor,&position);
        if(!out.read.returned || !Finite(position)){out.read.returned=false;return out;}
        auto input=parameters.offset;
        out.read=native::CallVoid(methods_.direction,anchor,&input,&offset);
    }else {
        const auto origin=LogicalPosition(transform);out.read=origin.read;
        if(!out.read.returned)return out;
        position=origin.value;position.y+=parameters.height*.5f;
        out.read=native::Call(methods_.transform_vector,&offset,transform,parameters.offset);
    }
    if(!out.read.returned || !Finite(offset)){out.read.returned=false;return out;}
    auto& shape=out.value;shape.attached=anchor!=nullptr;
    shape.center={position.x+offset.x,position.y+offset.y,position.z+offset.z};
    shape.vertical_extent=parameters.height*.5f;
    if(parameters.kind==HitShapeKind::Capsule) {
        shape.vertical_extent+=parameters.width;shape.effect_radius=parameters.width;
    }else shape.effect_radius=parameters.width*.5f;
    out.read.returned=Finite(shape.center)&&std::isfinite(shape.vertical_extent);
    return out;
}
Observed<Position3> HitGeometry::EffectOffset(void* transform,const HitShapeSnapshot& shape,Position3 attacker,
    float height,float angle) const {
    Observed<Position3> out;
    if(!std::isfinite(height))return out;
    if(height<0){out.read.returned=true;return out;}
    if(!transform || !Finite(attacker) || !Finite(shape.center) || !Nonnegative(shape.vertical_extent) ||
        !Nonnegative(shape.effect_radius) || !std::isfinite(angle))return out;
    float dx=attacker.x-shape.center.x,dz=attacker.z-shape.center.z;
    const auto magnitude=std::sqrt(dx*dx+dz*dz);
    if(!std::isfinite(magnitude))return out;
    if(magnitude>1.e-5f){dx/=magnitude;dz/=magnitude;}else dx=dz=0;
    // Source: atan2(deltaX,deltaZ), x=sin(angle), z=cos(angle).
    const float radians=static_cast<float>(std::atan2(static_cast<double>(dx),static_cast<double>(dz)))+angle*.01745329238474369f;
    const auto lower=shape.center.y-shape.vertical_extent,upper=shape.center.y+shape.vertical_extent;
    const auto desired=attacker.y+height;
    if(!std::isfinite(radians) || !std::isfinite(lower) || !std::isfinite(upper) || !std::isfinite(desired))return out;
    Position3 point{shape.center.x+static_cast<float>(std::sin(static_cast<double>(radians)))*shape.effect_radius,
        std::clamp(desired,lower,upper),
        shape.center.z+static_cast<float>(std::cos(static_cast<double>(radians)))*shape.effect_radius};
    if(!Finite(point))return out;
    out.read=native::CallVoid(methods_.inverse_point,transform,&point,&out.value);
    if(!Finite(out.value))out.read.returned=false;
    return out;
}
HitGeometryMethods ResolveHitGeometryMethods(MethodResolver& methods,const std::vector<DeclaredField>& fields) {
    HitGeometryMethods out;
    auto get=[&](const char* ns,const char* type,const char* name,const char* result,std::initializer_list<const char*> parameters) {
        try{return methods.Resolve(ns,type,name,result,parameters);}
        catch(const std::exception& e){out.binding_errors.push_back(std::string(type)+"."+name+": "+e.what());return Method{};}
    };
    out.transform_vector=get("RPG.GameCore","TransformComponent","TransformVector","RPG.MVector3",{"RPG.MVector3"});
    out.position=get("UnityEngine","Transform","get_position_Injected","System.Void",{"UnityEngine.Vector3&"});
    out.direction=get("UnityEngine","Transform","TransformDirection_Injected","System.Void",{"UnityEngine.Vector3&","UnityEngine.Vector3&"});
    out.inverse_point=get("UnityEngine","Transform","InverseTransformPoint_Injected","System.Void",{"UnityEngine.Vector3&","UnityEngine.Vector3&"});
    out.attach_point=get("RPG.GameCore","CharacterModelComponent","GetAttachPoint","UnityEngine.Transform",{"System.String","System.Boolean"});
    unsigned matches=0;
    for(const auto& f:fields)if(f.owner_namespace=="RPG.GameCore" && f.owner_name=="TransformComponent" &&
        f.name=="_WorldPos" && f.type=="RPG.MVector3" && f.flags_observed && !(f.flags&0x10)) {
        ++matches;out.logical_position_offset=f.offset;
    }
    out.logical_position_available=matches==1;
    if(!out.logical_position_available)out.binding_errors.push_back("TransformComponent._WorldPos: missing or ambiguous field");
    return out;
}
}
