#include <hksr/animation/ActorTiming.hpp>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace hksr::api {
ActorTiming::ActorTiming(ActorTimingServices calls) : calls_(std::move(calls)) {}
ActorTiming::~ActorTiming() = default;
std::optional<ActorTimingValue> ActorTiming::Layer(const std::string& name) const {
    return calls_.layer ? calls_.layer(name) : std::nullopt;
}
bool ActorTiming::SetLayer(const ActorTimingModel& model, const std::string& name, float weight, float duration) {
    if (name.empty() || name.find('\0') != std::string::npos || !std::isfinite(weight) || !std::isfinite(duration) ||
        duration < 0)
        throw std::invalid_argument("invalid official layer tween");
    return calls_.set_layer && calls_.set_layer(model, name, weight, duration);
}
std::optional<ActorTimingValue> ActorTiming::Speed() const {
    return calls_.speed ? calls_.speed() : std::nullopt;
}
bool ActorTiming::SetSpeed(const ActorTimingModel& model, float value) {
    if (!std::isfinite(value) || (value < 0 && value != -1))
        throw std::invalid_argument("invalid animator lock speed");
    return calls_.set_speed && calls_.set_speed(model, value);
}
ActorTimeScaleToken ActorTiming::Scale(const ActorTimeScaleInput& input, const std::string& key) {
    for (const auto value : {input.factor, input.delay, input.fade_in, input.fade_out})
        if (!std::isfinite(value) || value < 0)
            throw std::invalid_argument("invalid actor time scale");
    if (key.empty() || key.find('\0') != std::string::npos ||
        (input.duration && (!std::isfinite(*input.duration) || *input.duration <= 0)))
        throw std::invalid_argument("invalid actor time scale key/duration");
    return calls_.scale ? calls_.scale(input, key) : ActorTimeScaleToken{};
}
bool ActorTiming::Remove(ActorTimeScaleToken token, bool fade) {
    return !token.value || (calls_.remove && calls_.remove(token, fade));
}
std::optional<bool> ActorTiming::Contains(ActorTimeScaleToken token) const {
    return calls_.contains ? calls_.contains(token) : std::nullopt;
}
bool ActorTiming::Close() {
    return !calls_.close || calls_.close();
}
}
