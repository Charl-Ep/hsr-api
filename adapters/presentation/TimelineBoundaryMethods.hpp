#pragma once
#include <hksr/metadata/MetadataResolver.hpp>
namespace hksr::sdk_v1::detail {
struct TimelineBoundaryMethods {Method borrow,returned,removed,disposed,stop,resource,playing,recent,stop_record;};
TimelineBoundaryMethods ResolveTimelineBoundaryMethods(MethodResolver&);
}
