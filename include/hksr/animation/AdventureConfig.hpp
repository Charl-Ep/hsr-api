#pragma once
#include <memory>
#include "../metadata/RuntimeMetadata.hpp"
namespace hksr::sdk_v1 {
struct AdventureConfigMethods {
    Method load;
    std::size_t hidden_weapons_offset=0,event_paths_offset=0;
    bool has_hidden_weapons=false,has_event_paths=false;
    std::vector<std::string> binding_errors;
};
struct AdventureConfigSnapshot {
    ObjectResult hidden_weapons,event_paths; // nullable String[] is a valid empty input
};
// Cached managed configuration, NOT a CachedAssetLoader lease. Retain the
// returned object while consuming it; never mutate it or call ReleaseAsset.
class AdventureConfig {
public:
    HKSR_API explicit AdventureConfig(AdventureConfigMethods methods);
    HKSR_API ~AdventureConfig();
    HKSR_API AdventureConfig(const AdventureConfig&) noexcept;
    HKSR_API AdventureConfig& operator=(const AdventureConfig&) noexcept;
    HKSR_API ObjectResult Load(void* managed_path) const;
    HKSR_API AdventureConfigSnapshot Read(void* rooted_config) const;
private: std::shared_ptr<const AdventureConfigMethods> methods_;
};
// Pass fields from the resolved AdventureCharacterConfig and its parents,
// never an unrelated inventory. Each optional consumer field binds separately.
HKSR_API AdventureConfigMethods ResolveAdventureConfigMethods(MethodResolver& source,
    const std::vector<DeclaredField>& config_fields);

struct AdventureDataField {
    std::size_t offset=0;
    bool available=false;
    std::vector<std::string> binding_errors;
};
// Read-only baseline from this entity's AdventureCharacterDataComponent.
// Fields must come from that runtime class and parents, including field flags.
HKSR_API AdventureDataField ResolveAdventureDataField(const std::vector<DeclaredField>& fields);
HKSR_API ObjectResult ReadAdventureDataConfig(void* rooted_component,const AdventureDataField& field);
}
