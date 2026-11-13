#include "AnimationEventPolicyBindings.hpp"
#include <hksr/native/MetadataReader.hpp>
#include <stdexcept>
namespace hksr::api::detail {
Method ResolveEventPolicyCaster(MethodResolver& m) {
    return m.Resolve("RPG.GameCore", "TaskContext", "get_CasterEntity", "RPG.GameCore.GameEntity", {});
}
EventPerformer ReadEventPerformer(ObjectMetadata& metadata, const ComponentConfig& strings, void* task,
                                  const std::vector<DeclaredField>& fields, const std::string& performer_field) {
    std::string alias;
    const DeclaredField* field = nullptr;
    for (const auto& value : fields)
        if (value.name == performer_field && value.type == "RPG.GameCore.TargetEvaluator" && value.flags_observed &&
            !(value.flags & 0x10)) {
            if (field)
                throw std::runtime_error("ambiguous performer field");
            field = &value;
        }
    void* target = nullptr;
    if (!task || !field ||
        !native::CopyMemoryChecked(&target, static_cast<unsigned char*>(task) + field->offset, sizeof(target)) ||
        !target)
        throw std::runtime_error("performer missing or unreadable");
    const auto target_type = metadata.ReadObjectTypeName(target);
    if (target_type.name_space != "RPG.GameCore" || target_type.name != "TargetAlias")
        throw std::runtime_error("performer is not a literal alias");
    const auto target_fields = metadata.ReadObjectFields(target);
    field = nullptr;
    for (const auto& value : target_fields)
        if (value.name == "Alias" && value.type == "System.String" && value.flags_observed && !(value.flags & 0x10)) {
            if (field)
                throw std::runtime_error("ambiguous alias field");
            field = &value;
        }
    void* text = nullptr;
    if (!field || !native::CopyMemoryChecked(&text, static_cast<unsigned char*>(target) + field->offset, sizeof(text)))
        throw std::runtime_error("performer alias unreadable");
    if (text) {
        const auto value = strings.ReadText(text);
        if (!value.read.returned)
            throw std::runtime_error("performer alias text unreadable");
        alias = value.utf8;
    }
    return {alias == "Caster", std::move(alias)};
}
}
