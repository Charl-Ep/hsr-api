#pragma once
#include "../../../src/animation/catalog/CatalogAccess.hpp"
#include "../CatalogBindings.hpp"

namespace hksr::api::detail {
// Current official configuration projection. Request scheduling and managed
// reference ownership stay in the generic catalog access/consumer.
struct CatalogSchema : CatalogAccess {
    using CatalogAccess::CatalogAccess;
    Method Static(const char* type, const char* name, const char* result,
                  std::initializer_list<const char*> parameters = {}) {
        return methods.Resolve("RPG.GameCore", type, name, result, parameters, MethodKind::Static);
    }
    void* Config(const char* type, const std::string& path, Scope& scope) {
        Require(!path.empty(), "configuration path absent");
        auto input = scope.Keep(objects.NewStringUtf8(path.c_str()));
        auto name = std::string("Load") + type, returned = std::string("RPG.GameCore.") + type;
        return scope.Keep(
            Call(Static("GameCoreConfigLoader", name.c_str(), returned.c_str(), {"System.String"}), input));
    }
    template <class Row, class Decode>
    void Table(CatalogBatch<Row>& output, const char* table, const char* row_type, Decode decode) {
        Scope scope(*this);
        try {
            const auto returned =
                std::string("System.Collections.Generic.Dictionary<RPG.GameCore.CommonIndexKey,") + row_type + ">";
            auto dictionary = scope.Keep(Call(Static(table, "get_dataDict", returned.c_str())));
            auto values = scope.Keep(Call(Getter(metadata.ReadObjectType(dictionary), "get_Values"), dictionary));
            auto value_type = metadata.ReadObjectType(values);
            Require(native::Call(Instance(value_type, "get_Count", "System.Int32"), &output.declared_count, values)
                            .returned &&
                        output.declared_count >= 0,
                    "collection count unavailable");
            auto object_type = scope.Keep(objects.ManagedType("System", "Object"));
            auto array = scope.Keep(arrays.Create(object_type, output.declared_count));
            Require(native::CallVoid(Instance(value_type, "System.Collections.ICollection.CopyTo", "System.Void",
                                              {"System.Array", "System.Int32"}),
                                     values, array, 0)
                        .returned,
                    "collection copy failed");
            for (int i = 0; i < output.declared_count; ++i) {
                ++output.visited_count;
                try {
                    auto row = arrays.Get(array, i);
                    Require(row.call.returned && row.object, "table row unavailable");
                    output.rows.push_back(decode(row.object, metadata.ReadObjectFields(row.object)));
                } catch (const std::exception& error) {
                    output.issues.push_back({i, "row", error.what()});
                }
            }
            output.enumerated = true;
        } catch (const std::exception& error) {
            output.issues.push_back({-1, table, error.what()});
        }
    }
    void* GlobalVisualConfig(Scope& scope) {
        auto* config =
            scope.Keep(StaticValue(scope, "RPG.GameCore", "GameCoreConfigManager", "GameCoreConstValueConfig"));
        const auto actual = metadata.ReadObjectType(config);
        Require(actual.name_space == "RPG.GameCore" && actual.name == "GameCoreConstValue",
                "global visual source type differs");
        return config;
    }
    PresentationConfigInput Presentation(void* config, const std::vector<DeclaredField>& fields,
                                         const std::string& path) {
        PresentationConfigInput output;
        output.path = path;
        try {
            output.initial_state.value = String(config, fields, "InitAnimStateName");
            output.initial_state.available = true;
        } catch (const std::exception& error) {
            output.initial_state.error = error.what();
        }
        output.event_files = Strings(config, fields, "AnimEventConfigList");
        output.disabled_event_layers = Strings(config, fields, "DisableAnimEventLayers");
        return output;
    }
    struct DefinitionReference {
        void* object = nullptr;
        std::string declared_type, location;
    };
    DefinitionReference Definition(const DefinitionSource&, TypeRelationships&, Scope&);
    CatalogBatch<PlayerCatalogRow> Players();
    NpcMotionRow MotionRow(Method, std::uint32_t, std::string indexed_name = {});
    CatalogBatch<VisualBehaviorInput> VisualRows(void*, const char*, void* phase_config = nullptr);
    VisualConfigSources VisualSources(const std::string&, void*, bool, const std::string&);
    ActionConfigInput ReadActionConfig(ActionConfigKind kind, const std::string& path);
    TaskConfigInput ReadTaskConfig(const std::string& path);
    ActionConfigInput ReadActionConfigDetail(ActionConfigKind kind, const std::string& path, TaskConfigInput* tasks);
    CharacterCatalog ReadCharacters();
    TextInput ReadTextLanguage();
    CharacterDisplayName ReadCharacterDisplayName(CharacterNameKind kind, std::uint32_t id);
    TextInput ReadAdventureIdentity(const std::string& path);
    PresentationConfigInput ReadPresentationConfig(const std::string& path);
    NpcMotionIndex ReadNpcMotionIndex(const std::string& character);
    NpcMotionSources ReadNpcMotions(const std::string& character, const std::string& character_config);
    NpcMotionSources ReadNpcMotionsFromComponent(void* component);
    ConfigValue<float> ReadCharacterVisualRadius(const std::string& path);
    ConfigValue<HitEffectRules> ReadHitEffectRules(const std::string& path);
    VisualReactionInput ReadDefaultHitNames();
    CharacterHitInput ReadCharacterHitInput(const std::string& path);
    HitAnimationDefaults ReadHitAnimationDefaults();
    ReadyAnimationDefaults ReadReadyAnimationDefaults();
    TextInput ReadDefaultReadyTransition(std::int32_t from, std::int32_t to);
    PhaseVisualConfigSources ReadPhaseVisualConfigSources(const std::string& character, const PhaseVisualSource& source,
                                                          TypeRelationships& types);
    VisualConfigInput ReadDefaultVisualConfig();
    VisualConfigInput ReadCharacterVisualConfig(const std::string& path);
    VisualConfigSources ReadVisualConfigSources(const std::string& path);
    ActionDataVersion ReadActionDataVersion();
    std::string ReadCatalogRevision(const std::string& installed_revision);
    std::unique_ptr<ProjectileTemplate> ReadProjectileTemplate(const DefinitionSource& source,
                                                               TypeRelationships& types);
};
}
