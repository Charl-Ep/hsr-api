#include "CatalogSchema.hpp"

namespace hksr::api::detail {
ConfigValue<float> CatalogSchema::ReadCharacterVisualRadius(const std::string& path) {
    ConfigValue<float> out;
    auto& s = *this;
    Scope scope(s);
    try {
        auto* config = s.Config("CharacterConfig", path, scope);
        out.value = Read<float>(config, s.FieldFor(config, "VisualRadius", "System.Single"));
        out.available = true;
    } catch (const std::exception& e) {
        out.error = e.what();
    }
    return out;
}
}
