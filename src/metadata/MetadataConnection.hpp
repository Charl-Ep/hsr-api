#pragma once
#include <hksr/native/MetadataReader.hpp>
#include <hksr/runtime/GameVersion.hpp>

namespace hksr::sdk_v1::detail {
// Production adapter selection is not part of the raw diagnostic library.
native::MetadataConnection ConnectRuntimeMetadata(RuntimeVersion version);
}
