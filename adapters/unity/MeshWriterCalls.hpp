#pragma once
#include <hksr/unity/MeshSkinning.hpp>

namespace hksr::api::detail {
Observed<bool> WriteWeights(void*, void*, const FixedBoneWeights*, std::int32_t);
CallResult WriteBindposes(void*, void*, const BindposeMatrix*, std::int32_t);
}
