#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>

namespace hksr::api::detail {
enum class EffectFollowType { Plugin, Entity, Motion };
ObjectResult ResolveEffectFollowType(ManagedObjects&, EffectFollowType);
}
