#include "PartnerNativeBindings.hpp"
#include <stdexcept>
namespace hksr::api::detail {
ObjectResult ResolvePartnerComponentType(ManagedObjects& objects, PartnerComponent role) {
    switch (role) {
    case PartnerComponent::Group:
        return objects.ManagedType("", "OLABFALNLJM");
    case PartnerComponent::Transform:
        return objects.ManagedType("RPG.GameCore", "TransformComponent");
    case PartnerComponent::AnimationFollow:
        return objects.ManagedType("", "EAKGBLEMBLN");
    case PartnerComponent::Visibility:
        return objects.ManagedType("RPG.GameCore", "CharacterVisibleComponent");
    case PartnerComponent::BattleAnimation:
        return objects.ManagedType("", "CFFPEAALCPH");
    }
    throw std::invalid_argument("unknown partner component role");
}
PartnerControlMethods ResolvePartnerControlBindings(RuntimeMetadata& metadata) {
    const auto components = metadata.ReadFields("RPG.GameCore", "GameComponentBase", false);
    std::vector<std::string> errors;
    auto fields = [&](const char* type) {
        try {
            return metadata.ReadFields("", type, false);
        } catch (const std::exception& error) {
            errors.push_back(std::string(type) + ": " + error.what());
            return std::vector<DeclaredField>{};
        }
    };
    const auto attachment = fields("GDBHCNLLPAG");
    const auto visibility = fields("ANACIPLIMDE");
    auto out = ResolvePartnerControl(metadata, components, attachment, visibility);
    out.binding_errors.insert(out.binding_errors.end(), errors.begin(), errors.end());
    return out;
}
}
