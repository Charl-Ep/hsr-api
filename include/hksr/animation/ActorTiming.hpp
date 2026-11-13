#pragma once
#include "../Export.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace hksr::api {
struct EntityToken;
struct GameWorld;
class RuntimeMetadata;
struct ReferenceMethods;
class EntityAccess;
class ModelAccess;
class EntityOwner;
struct ActorTimingModel {
    std::uintptr_t model = 0, animator = 0, component = 0, art = 0, controller = 0;
    bool operator==(const ActorTimingModel& b) const {
        return model == b.model && animator == b.animator && component == b.component && art == b.art &&
               controller == b.controller;
    }
};
struct ActorTimingValue {
    ActorTimingModel model;
    float value = 0;
};
struct ActorTimeScaleInput {
    float factor = 1, delay = 0, fade_in = 0, fade_out = 0;
    std::optional<float> duration; // absent: infinite; native unscaled duration otherwise
};
struct ActorTimeScaleToken {
    std::uint64_t value = 0;
};
// Low-level provider seam. No clocks or interpolation are implemented here.
struct ActorTimingServices {
    std::function<std::optional<ActorTimingValue>(const std::string&)> layer;
    std::function<bool(const ActorTimingModel&, const std::string&, float, float)> set_layer;
    std::function<std::optional<ActorTimingValue>()> speed;
    std::function<bool(const ActorTimingModel&, float)> set_speed;
    std::function<ActorTimeScaleToken(const ActorTimeScaleInput&, const std::string&)> scale;
    std::function<bool(ActorTimeScaleToken, bool)> remove;
    std::function<std::optional<bool>(ActorTimeScaleToken)> contains;
    std::function<bool()> close;
};
class HKSR_API ActorTiming {
  public:
    explicit ActorTiming(ActorTimingServices);
    ~ActorTiming(); // no native calls; Close on the owning frame
    std::optional<ActorTimingValue> Layer(const std::string&) const;
    bool SetLayer(const ActorTimingModel&, const std::string&, float weight, float duration);
    std::optional<ActorTimingValue> Speed() const;
    bool SetSpeed(const ActorTimingModel&, float); // -1 is the official reset value
    ActorTimeScaleToken Scale(const ActorTimeScaleInput&, const std::string& unique_key);
    bool Remove(ActorTimeScaleToken, bool fade = false);
    std::optional<bool> Contains(ActorTimeScaleToken) const;
    bool Close();

  private:
    ActorTimingServices calls_;
};
}
