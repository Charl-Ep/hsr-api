#include <hksr/presentation/movement/MovementGeometry.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
MovementGeometryAccess ResolveMovementGeometryAccess(MethodResolver& methods,const std::vector<DeclaredField>& fields) {
    MovementGeometryAccess result;
    result.radius=methods.Resolve("RPG.GameCore","CharacterDataComponent","get_VisualRadius","System.Single",{});
    for(const auto& f:fields)if(f.name=="<LocalOffsetAsMoveTarget>k__BackingField" &&
        f.owner_namespace=="RPG.GameCore" && f.owner_name=="CharacterDataComponent") {
        if(result.local_offset || f.type!="RPG.MVector3" || !f.flags_observed || (f.flags&0x10) || f.offset<16)
            throw std::invalid_argument("movement target offset declaration differs");
        result.local_offset=f.offset;
    }
    if(!result.local_offset)throw std::invalid_argument("movement target offset declaration missing");
    return result;
}
Observed<MovementGeometry> MovementGeometryAccess::Read(void* component) const {
    Observed<MovementGeometry> result;
    if(!component)return result;
    result.read=native::Call(radius,&result.value.radius,component);
    if(!result.read.returned)return result;
    if(!native::CopyMemoryChecked(&result.value.local_offset,static_cast<unsigned char*>(component)+local_offset,sizeof(Position3)))
        result.read.returned=false;
    return result;
}
}
