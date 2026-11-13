#include <hksr/model/ActorVisualConfiguration.hpp>
#include <cmath>
#include <stdexcept>

namespace hksr::api {
namespace {
void Require(bool ok, const char* text) {
    if (!ok)
        throw std::runtime_error(text);
}
bool Text(const std::string& s) {
    return s.find('\0') == std::string::npos;
}
bool Finite(Position3 p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
}
struct ActorVisualConfiguration::State {
    ActorVisualConfigurationServices services;
    bool closed = false;
    explicit State(ActorVisualConfigurationServices s) : services(std::move(s)) {
        Require(bool(services.current) && bool(services.apply) && bool(services.emotion) && bool(services.close),
                "visual configuration services incomplete");
    }
    void Check() const {
        if (closed || !services.current())
            throw std::runtime_error("visual configuration character binding ended");
    }
};
ActorVisualConfiguration::ActorVisualConfiguration(ActorVisualConfigurationServices s)
    : state_(std::make_unique<State>(std::move(s))) {}
ActorVisualConfiguration::~ActorVisualConfiguration() = default;
bool ActorVisualConfiguration::Current() const {
    return !state_->closed && state_->services.current();
}
bool ActorVisualConfiguration::Current(VisualConfigurationComponent use) const {
    return Current() && (!state_->services.current_component || state_->services.current_component(use));
}
bool ActorVisualConfiguration::Current(const ActorVisualConfigurationInput& input) const {
    return Current() && (!state_->services.current_input || state_->services.current_input(input));
}
ActorVisualConfigurationResult ActorVisualConfiguration::Apply(const ActorVisualConfigurationInput& input) {
    auto& s = *state_;
    s.Check();
    if (input.reset && (input.radius || input.somato || input.effect_adaptations || input.graph_emotion))
        throw std::invalid_argument("reset cannot include visual overrides except explicit event files");
    if (input.radius && (!std::isfinite(*input.radius) || *input.radius < 0))
        throw std::invalid_argument("invalid visual radius");
    if (input.somato && *input.somato < 0)
        throw std::invalid_argument("invalid character somato type");
    if (input.effect_adaptations)
        for (const auto& e : *input.effect_adaptations)
            if (e.path.empty() || !Text(e.path) || !Text(e.attach_point) || (e.alias && !Text(*e.alias)) ||
                !Finite(e.position) || !Finite(e.rotation) || !std::isfinite(e.scale))
                throw std::invalid_argument("invalid effect adaptation");
    if (input.graph_emotion && !Text(*input.graph_emotion))
        throw std::invalid_argument("invalid graph emotion path");
    if (input.event_files)
        for (const auto& path : *input.event_files)
            if (path.empty() || !Text(path))
                throw std::invalid_argument("invalid animation event path");
    if (!input.reset && !input.radius && !input.somato && !input.effect_adaptations && !input.graph_emotion &&
        !input.event_files)
        return {};
    return s.services.apply(input);
}
bool ActorVisualConfiguration::EnableEmotion(bool enabled, float out, float in) {
    auto& s = *state_;
    s.Check();
    if (!std::isfinite(out) || !std::isfinite(in) || out < 0 || in < 0)
        throw std::invalid_argument("invalid emotion transition duration");
    return s.services.emotion(enabled, out, in);
}
bool ActorVisualConfiguration::Close() {
    auto& s = *state_;
    if (s.closed)
        return true;
    if (s.services.close()) {
        s.closed = true;
        return true;
    }
    return false;
}

}
