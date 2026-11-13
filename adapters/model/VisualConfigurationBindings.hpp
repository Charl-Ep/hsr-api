#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <functional>

namespace hksr::api::detail {
struct VisualConfigurationTypes {
    static constexpr const char* emotion = "PEPNFLNCDHG";
    static constexpr const char* events = "HHECHLOIEJO";
    static constexpr std::int32_t reset_somato = 14, emotion_source = 0;
};
enum class VisualConfigurationObject { Data, Emotion, EventComponent, EventConsumer, Adaptation, Somato };
ObjectResult ResolveVisualConfigurationObjectType(ManagedObjects&, VisualConfigurationObject);
ObjectResult AllocateVisualConfigurationObject(ManagedObjects&, VisualConfigurationObject);
bool MatchesVisualConfigurationObject(ObjectMetadata&, void*, VisualConfigurationObject);
// Declaration identities and layouts for the qualified game provider. No
// managed allocations, reference ownership or actor state is kept here.
struct VisualConfigurationBindings {
    DeclaredField overlay, radius, somato, adaptations, graph_emotion, event_files;
    DeclaredField path, alias, attach;
    std::size_t data_owner = 0, emotion_owner = 0, single_value = 0, somato_value = 0;
    std::size_t position = 0, rotation = 0, scale = 0;
    std::size_t consumer_paths = 0, consumer_layers = 0, consumer_ready = 0;
    Method adaptation_changed, get_adaptations, get_events, bind_events;
    Method emotion_enable, emotion_blendshape;
    std::string data_error, radius_error, somato_error, adaptations_error, adaptation_entries_error;
    std::string adaptation_refresh_error, graph_emotion_error, data_events_error, events_read_error;
    std::string consumer_error, emotion_error;
};
using VisualConfigurationFields = std::function<std::vector<DeclaredField>(const char*, const char*)>;
VisualConfigurationBindings ResolveVisualConfiguration(MethodResolver&, const VisualConfigurationFields&);
}
