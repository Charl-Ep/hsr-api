#pragma once
#include <hksr/presentation/events/AnimationEventDefinitions.hpp>
#include <algorithm>

namespace hksr::api::detail {
template <class Reader>
AnimationEventDefinition DecodeEventRecord(Reader& reader, void* object, AnimationEventPart part, int index) {
    AnimationEventDefinition row;
    row.part = part;
    row.index = index;
    const auto names = [&](const char* field, const char* type) {
        auto* entries = reader.Field(object, field, type);
        for (int i = 0, n = reader.Count(entries); i < n; ++i) {
            auto* entry = reader.Item(entries, i);
            if (!entry)
                continue;
            const auto name = reader.Text(entry, "Name");
            if (!name.empty() && std::find(row.states.begin(), row.states.end(), name) == row.states.end())
                row.states.push_back(name);
        }
    };
    if (part == AnimationEventPart::TimePoint) {
        reader.Type(object, "RPG.GameCore", "SingleAnimEventConfig");
        auto name = reader.Text(object, "AnimatorStateName");
        if (!name.empty())
            row.states.push_back(std::move(name));
        names("AnimatorStates", "RPG.GameCore.SingleAnimEntry[]");
    } else if (part == AnimationEventPart::Behavior) {
        reader.Type(object, "RPG.GameCore", "SingleAnimBehaviourEvent");
        row.behavior = reader.Text(object, "EventUniqueKey");
    } else {
        reader.Type(object, "RPG.GameCore", "AnimGroupEventConfig");
        names("States", "RPG.GameCore.AnimGroupEventEntry[]");
    }
    row.configurations = reader.Field(object,
                                      part == AnimationEventPart::GroupEnter  ? "OnEnter"
                                      : part == AnimationEventPart::GroupExit ? "OnExit"
                                                                              : "EventList",
                                      "RPG.GameCore.TaskConfig[]");
    return row;
}
}
