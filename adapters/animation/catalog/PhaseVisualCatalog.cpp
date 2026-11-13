#include "CatalogSchema.hpp"

namespace hksr::api::detail {
PhaseVisualConfigSources CatalogSchema::ReadPhaseVisualConfigSources(const std::string& character,
                                                                     const PhaseVisualSource& source,
                                                                     TypeRelationships& types) {
    PhaseVisualConfigSources output;
    output.source = source;
    auto& s = *this;
    Scope scope(s);
    void* phase = nullptr;
    std::string location = source.name, error;
    auto keep = [&](void* object) { return object ? scope.Keep(ObjectResult{{true, 0}, object}) : nullptr; };
    try {
        const auto selected = s.Definition(source, types, scope);
        phase = selected.object;
        location = selected.location;
        const auto& selected_type = selected.declared_type;
        Require(selected_type == "RPG.GameCore.CharacterPhaseAnimConfig",
                "phase path does not select a declared CharacterPhaseAnimConfig");
        if (phase) {
            const auto actual = s.metadata.ReadObjectTypeName(phase);
            Require(actual.name_space == "RPG.GameCore" && actual.name == "CharacterPhaseAnimConfig",
                    "selected data is not CharacterPhaseAnimConfig");
        } else {
            auto* base = s.Config("CharacterConfig", character, scope);
            phase = keep(Read<void*>(base, Field(s.metadata.ReadObjectFields(base), "PhaseAnimConfig",
                                                 "RPG.GameCore.CharacterPhaseAnimConfig")));
            location = character + ".PhaseAnimConfig";
        }
    } catch (const std::exception& e) {
        error = e.what();
        phase = nullptr;
        output.issues.push_back({-1, location, error});
    }
    output.visuals = s.VisualSources(character, phase, error.empty(), location);
    if (!error.empty())
        output.visuals.character.issues.push_back({-1, location, error});
    if (error.empty() && phase)
        try {
            const auto fields = s.metadata.ReadObjectFields(phase);
            auto capture = [&](auto& out, const char* name, auto read) {
                try {
                    out.value = read();
                    out.available = true;
                } catch (const std::exception& e) {
                    out.error = e.what();
                    output.issues.push_back({-1, name, e.what()});
                }
            };
            auto text = [&](TextInput& out, const char* name) {
                capture(out, name, [&] { return s.String(phase, fields, name); });
            };
            text(output.standby, "StandBy");
            text(output.revive, "Revive");
            output.reactions.present = true;
            auto& r = output.reactions;
            text(r.hit, "Hit");
            text(r.hit_h, "HitH");
            text(r.hit_fly, "HitFly");
            text(r.hit_fly_recover, "HitFlyRecover");
            text(r.fly_hit, "FlyHit");
            text(r.die, "Die");
            text(r.die_hit, "DieHit");
            text(r.die_fly, "DieFly");
            text(r.die_limbo, "DieLimbo");
            capture(r.force_hit_h, "ForceHitH",
                    [&] { return Read<std::uint8_t>(phase, Field(fields, "ForceHitH", "System.Boolean")) != 0; });
            capture(r.hit_weight, "OverrideHitAnimWeight",
                    [&] { return Read<float>(phase, Field(fields, "OverrideHitAnimWeight", "System.Single")); });
        } catch (const std::exception& e) {
            output.issues.push_back({-1, "phase fields", e.what()});
        }
    return output;
}
}
