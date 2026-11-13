#include <hksr/presentation/movement/MovementGeometry.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <stdexcept>

namespace hksr::api {
Observed<MovementGeometry> MovementGeometryAccess::Read(void* component) const {
    Observed<MovementGeometry> result;
    if (!component)
        return result;
    result.read = native::Call(radius, &result.value.radius, component);
    if (!result.read.returned)
        return result;
    if (!native::CopyMemoryChecked(&result.value.local_offset, static_cast<unsigned char*>(component) + local_offset,
                                   sizeof(Position3)))
        result.read.returned = false;
    return result;
}
}
