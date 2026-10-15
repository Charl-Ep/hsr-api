#pragma once
#include <hksr/metadata/MetadataResolver.hpp>
#include <hksr/runtime/GameVersion.hpp>
namespace hksr::sdk_v1::detail {
struct AnimatorLayout {std::uint32_t controller,playable_count,playable,constant;};
AnimatorLayout LocateAnimatorLayout(Method,RuntimeVersion);
}
