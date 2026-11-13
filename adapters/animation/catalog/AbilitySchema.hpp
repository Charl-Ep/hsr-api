#pragma once
#include "CatalogSchema.hpp"

namespace hksr::api::detail {
struct AbilityIssue {
    std::string error;
    AbilitySourceIssueKind kind = AbilitySourceIssueKind::ReadFailure;
};
struct AbilityNodeInput {
    std::optional<AnimationTaskSource> animation;
    std::optional<AnimationWaitSource> wait;
    std::optional<EffectRemovalSource> removal;
    std::optional<AbilityReferenceSource> reference;
    std::optional<SkillMappingSource> skill_mapping;
    std::optional<SkillContinuationSource> skill_continuation;
    std::vector<AbilityIssue> issues;
};
// One adapter for the current task schema. The consumer owns the traversal,
// budgets, cancellation, node cache and all managed-reference lifetimes.
class AbilitySchema {
    CatalogSchema& s;
    TypeRelationships& types;
    CatalogAccess::Scope& scope;
    struct Alias {
        TextInput value;
        AbilitySourceIssueKind issue = AbilitySourceIssueKind::ReadFailure;
    };
    std::map<void*, Alias> target_aliases;
    std::map<std::string, bool> config_types;

  public:
    struct Limitation final : std::runtime_error {
        AbilitySourceIssueKind kind;
        Limitation(AbilitySourceIssueKind value, const char* reason) : std::runtime_error(reason), kind(value) {}
    };
    static AbilitySourceIssueKind IssueKind(const std::exception& error) {
        const auto* limitation = dynamic_cast<const Limitation*>(&error);
        return limitation ? limitation->kind : AbilitySourceIssueKind::ReadFailure;
    }
    AbilitySchema(CatalogSchema& source, TypeRelationships& relation, CatalogAccess::Scope& owner)
        : s(source), types(relation), scope(owner) {}
    bool IsConfigType(const std::string& qualified);
    bool IsConfigObject(void* object);
    AbilityNodeInput ReadNode(void* object, std::vector<DeclaredField>& fields);
    Method LoadMethod();
};
}
