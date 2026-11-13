#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <functional>
#include <string>

namespace hksr::api::detail {
using TimingFields = std::function<std::vector<DeclaredField>(const char*, const char*)>;
using TimingType = std::function<void*(const char*, const char*)>;
struct TimingBindings {
    void *model_type = nullptr, *battle_type = nullptr, *adventure_type = nullptr;
    Method battle_animator, adventure_mock, mock_animator, controller, layer_index, layer_weight, battle_tween,
        adventure_tween;
    Method scale_infinite, scale_finite, remove, contains;
    std::size_t clock_offset = 0, speed_offset = 0;
    std::string animator_error, layer_error, battle_error, adventure_error, battle_tween_error, adventure_tween_error;
    std::string speed_error, clock_error, infinite_error, finite_error;
};
TimingBindings ResolveTimingBindings(MethodResolver&, const TimingFields&, const TimingType&);
}
