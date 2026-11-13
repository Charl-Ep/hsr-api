#pragma once
#include <hksr/metadata/MetadataResolver.hpp>
#include <array>

namespace hksr::api::detail {
std::array<Method, 3> ResolveEntityLifecycleEntries(MethodResolver&);
}
