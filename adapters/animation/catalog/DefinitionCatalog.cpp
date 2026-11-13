#include "CatalogSchema.hpp"
#include <hksr/presentation/projectiles/ProjectileTemplate.hpp>

namespace hksr::api::detail {
CatalogSchema::DefinitionReference CatalogSchema::Definition(const DefinitionSource& source, TypeRelationships& types,
                                                             Scope& scope) {
    Require(!source.name.empty() && source.name.find('\0') == std::string::npos && !source.path.empty(),
            "definition source is empty or invalid");
    const char* getter = nullptr;
    const char* root_type = nullptr;
    switch (source.root) {
    case DefinitionRoot::Ability:
        getter = "GetTurnBasedAbility";
        root_type = "RPG.GameCore.TurnBasedAbilityConfig";
        break;
    case DefinitionRoot::GlobalModifier:
        getter = "GetGlobalModifierConfig";
        root_type = "RPG.GameCore.TurnBasedModifierConfig";
        break;
    case DefinitionRoot::GlobalTemplate:
        getter = "GetGlobalTaskListTemplate";
        root_type = "RPG.GameCore.TaskListTemplate";
        break;
    default:
        throw std::invalid_argument("unknown definition root");
    }
    auto* name = scope.Keep(objects.NewStringUtf8(source.name.c_str()));
    DefinitionReference result{scope.Keep(Call(Static("AbilityJsonTable", getter, root_type, {"System.String"}), name)),
                               root_type, source.name};
    const auto keep = [&](void* object) { return object ? scope.Keep(ObjectResult{{true, 0}, object}) : nullptr; };
    const auto reference = [&](const std::string& type) {
        const auto dot = type.rfind('.');
        Require(dot != std::string::npos && type.find_first_of("<[]>&*") == std::string::npos,
                "definition path requires a concrete reference");
        Require(
            !types.TypeDerivesFrom(type.substr(0, dot).c_str(), type.substr(dot + 1).c_str(), "System", "ValueType"),
            "definition path cannot reinterpret a value-type field");
    };
    for (const auto& step : source.path) {
        try {
            Require(result.object != nullptr, "null intermediate node");
            const auto fields = metadata.ReadObjectFields(result.object);
            DeclaredField selected;
            unsigned matches = 0;
            for (const auto& f : fields)
                if (f.owner_namespace + "." + f.owner_name == step.declaring_type && f.name == step.field &&
                    f.flags_observed && !(f.flags & 0x10)) {
                    selected = f;
                    ++matches;
                }
            Require(matches == 1, "definition path field is absent or ambiguous");
            result.location += "." + step.field;
            if (step.selection == ConfigSelection::Reference) {
                result.declared_type = selected.type;
                reference(selected.type);
                result.object = keep(Read<void*>(result.object, selected));
            } else if (step.selection == ConfigSelection::Index) {
                const auto& type = selected.type;
                const std::string prefix = "System.Collections.Generic.List<";
                const bool array = type.size() > 2 && type.substr(type.size() - 2) == "[]";
                const bool list = type.compare(0, prefix.size(), prefix) == 0 && type.back() == '>';
                Require(array || list, "definition index source is not an array or List");
                const auto element = array ? type.substr(0, type.size() - 2)
                                           : type.substr(prefix.size(), type.size() - prefix.size() - 1);
                result.declared_type = element;
                reference(element);
                auto* sequence = keep(Read<void*>(result.object, selected));
                Require(sequence && step.index >= 0, "definition sequence/index is absent");
                result.location += "[" + std::to_string(step.index) + "]";
                if (array) {
                    const auto count = arrays.Length(sequence);
                    Require(count.read.returned && step.index < count.value, "definition array index outside source");
                    const auto value = arrays.Get(sequence, step.index);
                    Require(value.call.returned, "definition array item unread");
                    result.object = keep(value.object);
                } else {
                    const auto declared = metadata.ReadObjectType(sequence);
                    const auto value =
                        Call(Instance(declared, "get_Item", element.c_str(), {"System.Int32"}), sequence, step.index);
                    Require(value.call.returned, "definition list item unread");
                    result.object = keep(value.object);
                }
            } else if (step.selection == ConfigSelection::StringKey) {
                Require(selected.type.find("System.Collections.Generic.Dictionary<System.String,") == 0,
                        "definition key source is not a string dictionary");
                auto* dictionary = keep(Read<void*>(result.object, selected));
                Require(dictionary != nullptr, "definition dictionary is null");
                const auto declared = metadata.ReadObjectType(dictionary);
                Method item{};
                std::string returned;
                unsigned found = 0;
                for (const auto& m : declared.methods)
                    if (m.name == "get_Item" && !m.is_static &&
                        m.parameters == std::vector<std::string>{"System.String"}) {
                        item = m.entry;
                        returned = m.result;
                        ++found;
                    }
                Require(found == 1, "definition dictionary getter is absent or ambiguous");
                result.declared_type = returned;
                reference(returned);
                auto* key = scope.Keep(objects.NewStringUtf8(step.key.c_str()));
                const auto value = Call(item, dictionary, key);
                Require(value.call.returned, "definition dictionary item unread");
                result.object = keep(value.object);
                result.location += "[" + step.key + "]";
            } else
                throw std::runtime_error("unknown definition path selection");
        } catch (const std::exception& e) {
            throw std::runtime_error(result.location + ": " + e.what());
        }
    }
    return result;
}
std::unique_ptr<ProjectileTemplate> CatalogSchema::ReadProjectileTemplate(const DefinitionSource& source,
                                                                          TypeRelationships& types) {
    auto& s = *this;
    Scope scope(s);
    const auto selected = s.Definition(source, types, scope);
    Require(selected.object != nullptr, "projectile template is null");
    const auto actual = s.metadata.ReadObjectTypeName(selected.object);
    Require(actual.name_space == "RPG.GameCore" &&
                (actual.name == "ProjectileData" || actual.name == "LinearProjectileData" ||
                 actual.name == "RandomTraceProjectileData"),
            "selected definition is not a qualified projectile template");
    return std::make_unique<ProjectileTemplate>(s.references, selected.object);
}
}
