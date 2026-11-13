#include "AbilitySchema.hpp"

namespace hksr::api::detail {
bool AbilitySchema::IsConfigType(const std::string& qualified) {
    auto found = config_types.find(qualified);
    if (found != config_types.end())
        return found->second;
    if (qualified.find_first_of("<[]>&*") != std::string::npos)
        throw Limitation(AbilitySourceIssueKind::Unsupported, "unsupported constructed task declaration");
    const auto split = qualified.find_last_of('.');
    const auto space = split == std::string::npos ? std::string() : qualified.substr(0, split);
    const auto name = split == std::string::npos ? qualified : qualified.substr(split + 1);
    const bool result = types.TypeDerivesFrom(space.c_str(), name.c_str(), "RPG.GameCore", "JsonConfig");
    config_types.emplace(qualified, result);
    return result;
}
bool AbilitySchema::IsConfigObject(void* object) {
    const auto type = s.metadata.ReadObjectTypeName(object);
    return IsConfigType(type.name_space + "." + type.name);
}
AbilityNodeInput AbilitySchema::ReadNode(void* object, std::vector<DeclaredField>& fields) {
    AbilityNodeInput node;
    try {
        scope.Keep({{true, 0}, object});
        const auto type = s.metadata.ReadObjectTypeName(object);
        const auto name = type.name_space + "." + type.name;
        Require(IsConfigType(name), "task graph node is not JsonConfig-derived");
        fields = s.metadata.ReadObjectFields(object);
        auto capture = [&](auto& value, const char* field, auto read) {
            try {
                value.value = read();
                value.available = true;
            } catch (const std::exception& error) {
                value.error = error.what();
                node.issues.push_back({std::string(field) + ": " + error.what(), IssueKind(error)});
            }
        };
        auto target_alias = [&]() {
            if (std::none_of(fields.begin(), fields.end(),
                             [](const auto& field) { return field.name == "TargetType"; }))
                throw Limitation(AbilitySourceIssueKind::NotLiteral, "task does not declare a literal target");
            const auto target = Read<void*>(object, Field(fields, "TargetType", "RPG.GameCore.TargetEvaluator"));
            if (!target)
                throw Limitation(AbilitySourceIssueKind::NotLiteral, "target is null, not a literal Caster");
            auto found = target_aliases.find(target);
            if (found == target_aliases.end()) {
                Alias alias;
                auto& value = alias.value;
                try {
                    scope.Keep({{true, 0}, target});
                    const auto target_type = s.metadata.ReadObjectTypeName(target);
                    if (target_type.name_space != "RPG.GameCore" || target_type.name != "TargetAlias")
                        throw Limitation(AbilitySourceIssueKind::NotLiteral, "target is not a literal alias");
                    value = {true, s.String(target, s.metadata.ReadObjectFields(target), "Alias"), {}};
                } catch (const std::exception& e) {
                    value.error = e.what();
                    alias.issue = IssueKind(e);
                }
                found = target_aliases.emplace(target, std::move(alias)).first;
            }
            if (!found->second.value.available) {
                if (found->second.issue != AbilitySourceIssueKind::ReadFailure)
                    throw Limitation(found->second.issue, found->second.value.error.c_str());
                throw std::runtime_error(found->second.value.error);
            }
            return found->second.value.value;
        };
        if (name == "RPG.GameCore.TriggerAnimState" || name == "RPG.GameCore.TriggerAnimStateWithMove" ||
            name == "RPG.GameCore.MoveToTargetList") {
            AnimationTaskSource entry;
            entry.kind = name == "RPG.GameCore.TriggerAnimState"           ? AnimationTaskKind::State
                         : name == "RPG.GameCore.TriggerAnimStateWithMove" ? AnimationTaskKind::StateWithMove
                                                                           : AnimationTaskKind::StateWithTargetList;
            // Discover the literal animation input, not an executable
            // movement/target graph. MoveToTargetPosition has no such input.
            capture(entry.state, "AnimStateName", [&] { return s.String(object, fields, "AnimStateName"); });
            capture(entry.logic_kind, "AnimLogicState", [&] {
                return Read<int>(object, Field(fields, "AnimLogicState", "RPG.GameCore.AnimStateLogicType"));
            });
            capture(entry.enabled, "TaskEnabled",
                    [&] { return Read<unsigned char>(object, Field(fields, "TaskEnabled", "System.Boolean")) != 0; });
            if (entry.kind == AnimationTaskKind::StateWithMove)
                capture(entry.waits, "WaitAnimState", [&] {
                    return Read<unsigned char>(object, Field(fields, "WaitAnimState", "System.Boolean")) != 0;
                });
            // Optional relationship information must not hide an otherwise
            // discoverable animation with a computed or missing target.
            try {
                entry.target_alias = {true, target_alias(), {}};
            } catch (const std::exception& e) {
                entry.target_alias.error = e.what();
                if (IssueKind(e) == AbilitySourceIssueKind::ReadFailure)
                    node.issues.push_back({e.what()});
            }
            node.animation = std::move(entry);
        }
        if (name == "RPG.GameCore.WaitAnimState") {
            AnimationWaitSource entry;
            capture(entry.state, "AnimStateName", [&] { return s.String(object, fields, "AnimStateName"); });
            capture(entry.target_alias, "TargetType", target_alias);
            capture(entry.enabled, "TaskEnabled",
                    [&] { return Read<unsigned char>(object, Field(fields, "TaskEnabled", "System.Boolean")) != 0; });
            node.wait = std::move(entry);
        }
        if (name == "RPG.GameCore.RemoveEffect") {
            EffectRemovalSource entry;
            auto& value = entry.input;
            capture(value.effect_path, "EffectPath", [&] { return s.String(object, fields, "EffectPath"); });
            capture(value.path_key, "EffectPathKey", [&] { return s.String(object, fields, "EffectPathKey"); });
            capture(value.target_alias, "TargetType", target_alias);
            capture(value.enabled, "TaskEnabled",
                    [&] { return Read<unsigned char>(object, Field(fields, "TaskEnabled", "System.Boolean")) != 0; });
            node.removal = std::move(entry);
        }
        if (name == "RPG.GameCore.TriggerAbility") {
            AbilityReferenceSource entry;
            capture(entry.enabled, "TaskEnabled",
                    [&] { return Read<unsigned char>(object, Field(fields, "TaskEnabled", "System.Boolean")) != 0; });
            capture(entry.target, "AbilityName", [&] {
                // DynamicString is a reference type. Only a literal value
                // is a discovery edge; never
                // evaluate its key or execute the containing ability.
                auto value = Read<void*>(object, Field(fields, "AbilityName", "RPG.GameCore.DynamicString"));
                if (!value)
                    throw Limitation(AbilitySourceIssueKind::NotLiteral, "ability name absent");
                const auto declared = s.metadata.ReadObjectFields(value);
                if (Read<unsigned char>(value, Field(declared, "Custom", "System.Boolean")))
                    throw Limitation(AbilitySourceIssueKind::NotLiteral,
                                     "dynamic ability name requires runtime context");
                return s.String(value, declared, "Value");
            });
            node.reference = std::move(entry);
        }
        if (name == "RPG.GameCore.SetControlSkillMapping") {
            SkillMappingSource entry;
            capture(entry.skill, "SkillTriggerKey", [&] { return s.String(object, fields, "SkillTriggerKey"); });
            capture(entry.target_alias, "TargetType", target_alias);
            capture(entry.control, "ControlSkillType", [&] {
                return Read<int>(object, Field(fields, "ControlSkillType", "RPG.GameCore.ControlSkillType"));
            });
            capture(entry.enabled, "TaskEnabled",
                    [&] { return Read<unsigned char>(object, Field(fields, "TaskEnabled", "System.Boolean")) != 0; });
            node.skill_mapping = std::move(entry);
        }
        if (name == "RPG.GameCore.UseSkillOneMore") {
            SkillContinuationSource entry;
            capture(entry.control, "SkillType",
                    [&] { return Read<int>(object, Field(fields, "SkillType", "RPG.GameCore.ControlSkillType")); });
            capture(entry.child, "ChildSkillIndex",
                    [&] { return Read<int>(object, Field(fields, "ChildSkillIndex", "System.Int32")); });
            capture(entry.enabled, "TaskEnabled",
                    [&] { return Read<unsigned char>(object, Field(fields, "TaskEnabled", "System.Boolean")) != 0; });
            node.skill_continuation = std::move(entry);
        }
    } catch (const std::exception& error) {
        node.issues.push_back({error.what(), IssueKind(error)});
    }
    return node;
}
Method AbilitySchema::LoadMethod() {
    return s.Static("AbilityJsonTable", "GetTurnBasedAbility", "RPG.GameCore.TurnBasedAbilityConfig",
                    {"System.String"});
}
}
