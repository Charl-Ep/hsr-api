#pragma once
#include <hksr/native/MetadataReader.hpp>

namespace hksr::api::detail {
// Production adapter selection is not part of the raw diagnostic library.
native::MetadataConnection ConnectRuntimeMetadata();
}
