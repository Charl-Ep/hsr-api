#pragma once
#include <hksr/metadata/MetadataResolver.hpp>
namespace hksr::api::detail {
struct AnimatorLayout {
    std::uint32_t controller, playable_count, playable, constant;
};
AnimatorLayout LocateAnimatorLayout(Method);
}
