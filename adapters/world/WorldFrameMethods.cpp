#include <hksr/world/WorldFrames.hpp>
#include "WorldFrameBindings.hpp"
#include <stdexcept>
#include <string>

namespace hksr::api {
WorldFrameMethods detail::ResolveWorldFrameBindings(RuntimeMetadata& metadata) {
    std::vector<DeclaredField> fields;
    std::vector<std::string> field_errors;
    try {
        const auto observed = metadata.ObserveFields(metadata.FindType("RPG.GameCore", "GameWorld"));
        bool incomplete_target = false;
        for (const auto& row : observed.fields) {
            const auto& field = row.declaration;
            if (!row.error.empty())
                field_errors.push_back(field.name + ": " + row.error);
            // This capability has an explicit declaring-type/name anchor.
            // Unknown unrelated rows do not revoke a readable named target.
            if (field.name != "BindAdventurePhase" ||
                (!field.type.empty() && field.type != "RPG.Client.AdventurePhase") ||
                (field.flags_observed && (field.flags & 0x10)))
                continue;
            if (field.type.empty() || !row.offset_observed)
                incomplete_target = true;
            else
                fields.push_back(field);
        }
        if (incomplete_target)
            fields.clear();
        if (!observed.iteration_error.empty())
            field_errors.push_back(observed.iteration_error);
    } catch (const std::exception& e) {
        field_errors.push_back(e.what());
    }
    auto result = ResolveWorldFrameMethods(metadata, fields);
    result.binding_errors.insert(result.binding_errors.end(), field_errors.begin(), field_errors.end());
    return result;
}
WorldFrameMethods ResolveWorldFrameMethods(MethodResolver& source, const std::vector<DeclaredField>& fields) {
    WorldFrameMethods result;
    auto optional = [&](const char* name, const char* returns,
                        std::initializer_list<const char*> parameters = std::initializer_list<const char*>{}) {
        try {
            return source.Resolve("RPG.GameCore", "GameWorld", name, returns, parameters);
        } catch (const std::exception& error) {
            result.binding_errors.push_back(std::string(name) + ": " + error.what());
            return Method{};
        }
    };
    result.late_update = optional("LateUpdate", "System.Void", {"System.Single"});
    result.manager = optional("get_EntityManagerRef", "RPG.GameCore.EntityManager");
    result.disposing = optional("get_IsDisposing", "System.Boolean");
    result.battle = optional("get_IsBattleGameWorld", "System.Boolean");
    result.paused = optional("get_IsPauseTick", "System.Boolean");
    result.in_tick = optional("get_IsInTick", "System.Boolean");
    unsigned matches = 0;
    for (const auto& field : fields)
        if (field.owner_namespace == "RPG.GameCore" && field.owner_name == "GameWorld" &&
            field.name == "BindAdventurePhase" && field.type == "RPG.Client.AdventurePhase" &&
            !(field.flags_observed && (field.flags & 0x10))) {
            ++matches;
            result.phase_offset = field.offset;
        }
    result.has_phase_offset = matches == 1;
    if (!result.has_phase_offset)
        result.binding_errors.push_back("BindAdventurePhase: missing or ambiguous declared field");
    return result;
}
}
