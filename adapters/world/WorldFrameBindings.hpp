#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/world/WorldFrames.hpp>

namespace hksr::api::detail {
HKSR_API WorldFrameMethods ResolveWorldFrameBindings(RuntimeMetadata&);
}
