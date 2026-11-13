#include <hksr/animation/ActorTiming.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace hksr::api;
void TestActorTimingNative();
namespace {
void Check(bool ok) {
    if (!ok)
        throw std::runtime_error("actor timing contract");
}
template <class F> void Reject(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Check(rejected);
}
}
int main() {
    try {
        TestActorTimingNative();
        ActorTimingModel model{1, 2, 3, 4};
        float layer = 0, speed = -1, duration = 0;
        bool live = true, removed = false, closed = false;
        ActorTimingServices services;
        services.layer = [&](const auto& name) -> std::optional<ActorTimingValue> {
            return live && name == "Hit" ? std::optional<ActorTimingValue>{{model, layer}} : std::nullopt;
        };
        services.set_layer = [&](const auto& expected, const auto& name, float value, float seconds) {
            Check(expected == model && name == "Hit");
            layer = value;
            duration = seconds;
            return live;
        };
        services.speed = [&]() -> std::optional<ActorTimingValue> {
            return live ? std::optional<ActorTimingValue>{{model, speed}} : std::nullopt;
        };
        services.set_speed = [&](const auto& expected, float value) {
            Check(expected == model);
            speed = value;
            return live;
        };
        services.scale = [](const ActorTimeScaleInput& input, const auto& key) {
            Check(input.factor == .3f && input.duration == 2 && input.fade_in == .2f && key == "owned");
            return ActorTimeScaleToken{7};
        };
        services.remove = [&](auto token, bool fade) {
            Check(token.value == 7 && !fade);
            removed = true;
            return true;
        };
        services.contains = [&](auto token) -> std::optional<bool> {
            Check(token.value == 7);
            return !removed;
        };
        services.close = [&] {
            closed = true;
            return true;
        };
        ActorTiming api(std::move(services));
        Check(api.SetLayer(model, "Hit", .8f, .7f) && layer == .8f && duration == .7f);
        Check(api.SetSpeed(model, .4f) && speed == .4f);
        Check(api.SetSpeed(model, -1) && speed == -1);
        ActorTimeScaleInput input;
        input.factor = .3f;
        input.duration = 2.0f;
        input.fade_in = .2f;
        const auto token = api.Scale(input, "owned");
        Check(token.value == 7 && *api.Contains(token));
        Check(api.Remove(token) && !*api.Contains(token));
        live = false;
        Check(!api.Layer("Hit") && !api.Speed());
        Reject([&] { api.SetLayer(model, "Hit", 1, -1); });
        Reject([&] { api.SetSpeed(model, -2); });
        input.factor = std::numeric_limits<float>::quiet_NaN();
        Reject([&] { api.Scale(input, "owned"); });
        Check(api.Close() && closed);
        std::cout << "Actor timing typed provider contracts passed; no native/game qualification.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
