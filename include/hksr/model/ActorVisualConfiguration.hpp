#pragma once
#include "../entity/EntityLifecycle.hpp"
#include <hksr/math/SpatialTypes.hpp>
#include "../managed/ManagedArrays.hpp"
#include "../metadata/RuntimeMetadata.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace hksr::api {
struct EffectAdaptionEntry {
    std::string path;
    std::optional<std::string> alias;
    std::string attach_point;
    Position3 position, rotation;
    float scale = 1;
};
struct ActorVisualConfigurationInput {
    bool reset = false;
    std::optional<float> radius;
    std::optional<std::int32_t> somato;
    std::optional<std::vector<EffectAdaptionEntry>> effect_adaptations;
    // Empty string clears the override and selects CharacterConfig defaults.
    std::optional<std::string> graph_emotion;
    std::optional<std::vector<std::string>> event_files;
};
struct ActorVisualConfigurationResult {
    std::vector<std::string> unsupported_fields;
};
enum class VisualConfigurationComponent { Data, Events, Emotion };
struct ActorVisualConfigurationBinding {
    EntityOrigin owner;
    std::uintptr_t model = 0;
    void* data = nullptr;
    void* emotion = nullptr;
    // Verify ownership/world and the exact current model identity.
    // No final model-load, changing-state or Art-ready prerequisite.
    std::function<bool()> current;
    void* event_consumer = nullptr; // exact current HHECHLOIEJO of this model
    bool has_event_component = false;
    // Validate only the component used by an operation. A successful null
    // component is distinct from an unavailable lookup, which must throw.
    // If omitted, current must also qualify the supplied component inputs.
    std::function<bool(VisualConfigurationComponent)> current_component;
};
struct ActorVisualConfigurationServices {
    std::function<bool()> current;
    std::function<ActorVisualConfigurationResult(const ActorVisualConfigurationInput&)> apply;
    std::function<bool(bool, float, float)> emotion;
    std::function<bool()> close;
    std::function<bool(VisualConfigurationComponent)> current_component;
    std::function<bool(const ActorVisualConfigurationInput&)> current_input;
};
// Explicit owned-character configuration. Reset clears this visual subset to the
// official default sentinels; it does not restore an arbitrary previous writer.
// With CharacterData, ordinary reset rebinds events only after a nonempty
// override. Pass explicit event_files when the consumer must be rebound after
// an empty override or after replacing the model.
// Without CharacterDataComponent, four data overrides are reported unsupported;
// event files and emotion remain independent. Event reset then restores the
// exact paths/layers captured before its first event operation, not battle defaults.
// No HUD, hit box, crosshair, gameplay tasks, hooks or automatic retries.
class ActorVisualConfiguration {
  public:
    HKSR_API explicit ActorVisualConfiguration(ActorVisualConfigurationServices);
    HKSR_API ~ActorVisualConfiguration();
    HKSR_API bool Current() const;
    // Identity only, not adaptation availability. An unread component lookup
    // throws; it is not a request to recreate the binding indefinitely.
    HKSR_API bool Current(VisualConfigurationComponent) const;
    // Uses the request's actual component dependencies, including conditional
    // event reset. Does not apply the requested configuration.
    HKSR_API bool Current(const ActorVisualConfigurationInput&) const;
    HKSR_API ActorVisualConfigurationResult Apply(const ActorVisualConfigurationInput&);
    // false: this character has no supported PEPN emotion component. The
    // task's separate Mono-component fallback is not implemented by this API.
    HKSR_API bool EnableEmotion(bool enabled, float transit_out, float transit_in);
    HKSR_API bool Close();

  private:
    struct State;
    std::unique_ptr<State> state_;
};
// Prepare metadata once on the engine thread. The providers must remain alive
// through all returned bindings. Bind does not repeat metadata discovery.
class ActorVisualConfigurationFactory {
  public:
    using Fields = std::function<std::vector<DeclaredField>(const char*, const char*)>;
    HKSR_API ActorVisualConfigurationFactory(MethodResolver&, ObjectMetadata&, ManagedObjects&, ReferenceMethods,
                                             ManagedArrays, Fields);
    HKSR_API ~ActorVisualConfigurationFactory();
    HKSR_API std::unique_ptr<ActorVisualConfiguration> Bind(ActorVisualConfigurationBinding);
    HKSR_API bool Close();

  private:
    struct State;
    std::shared_ptr<State> state_;
};
}
