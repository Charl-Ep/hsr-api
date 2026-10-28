#include "CatalogReader.hpp"

namespace hksr::sdk_v1 {
ConfigValue<float> OfficialCatalog::ReadCharacterVisualRadius(const std::string& path) {
    ConfigValue<float> out;auto& s=*state_;State::Scope scope(s);
    try {
        auto* config=s.Config("CharacterConfig",path,scope);
        out.value=Read<float>(config,Field(s.metadata.ReadObjectFields(config),"VisualRadius","System.Single"));
        out.available=true;
    }catch(const std::exception& e){out.error=e.what();}
    return out;
}
}
