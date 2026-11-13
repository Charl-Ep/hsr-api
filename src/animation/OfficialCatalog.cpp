#include "catalog/CatalogReader.hpp"
#include <hksr/presentation/projectiles/ProjectileTemplate.hpp>

namespace hksr::api {
OfficialCatalog::OfficialCatalog(MethodResolver& methods, ObjectMetadata& metadata, ManagedObjects& objects,
                                 ReferenceMethods references, ManagedArrays arrays, ComponentConfig text)
    : owner_(new std::shared_ptr<State>(std::make_shared<State>(methods, metadata, objects, references, arrays, text))),
      state_(owner_->get()) {}
OfficialCatalog::~OfficialCatalog() {
    delete owner_;
}
std::size_t OfficialCatalog::UnresolvedReferences() const {
    return state_->pending.size();
}
ActionConfigInput OfficialCatalog::ReadActionConfig(ActionConfigKind kind, const std::string& path) {
    return state_->ReadActionConfig(kind, path);
}
TaskConfigInput OfficialCatalog::ReadTaskConfig(const std::string& path) {
    return state_->ReadTaskConfig(path);
}
CharacterCatalog OfficialCatalog::ReadCharacters() {
    return state_->ReadCharacters();
}
TextInput OfficialCatalog::ReadTextLanguage() {
    return state_->ReadTextLanguage();
}
CharacterDisplayName OfficialCatalog::ReadCharacterDisplayName(CharacterNameKind kind, std::uint32_t id) {
    return state_->ReadCharacterDisplayName(kind, id);
}
TextInput OfficialCatalog::ReadAdventureIdentity(const std::string& path) {
    return state_->ReadAdventureIdentity(path);
}
PresentationConfigInput OfficialCatalog::ReadPresentationConfig(const std::string& path) {
    return state_->ReadPresentationConfig(path);
}
NpcMotionIndex OfficialCatalog::ReadNpcMotionIndex(const std::string& character) {
    return state_->ReadNpcMotionIndex(character);
}
NpcMotionSources OfficialCatalog::ReadNpcMotions(const std::string& character, const std::string& character_config) {
    return state_->ReadNpcMotions(character, character_config);
}
NpcMotionSources OfficialCatalog::ReadNpcMotionsFromComponent(void* component) {
    return state_->ReadNpcMotionsFromComponent(component);
}
ConfigValue<float> OfficialCatalog::ReadCharacterVisualRadius(const std::string& path) {
    return state_->ReadCharacterVisualRadius(path);
}
ConfigValue<HitEffectRules> OfficialCatalog::ReadHitEffectRules(const std::string& path) {
    return state_->ReadHitEffectRules(path);
}
VisualReactionInput OfficialCatalog::ReadDefaultHitNames() {
    return state_->ReadDefaultHitNames();
}
CharacterHitInput OfficialCatalog::ReadCharacterHitInput(const std::string& path) {
    return state_->ReadCharacterHitInput(path);
}
HitAnimationDefaults OfficialCatalog::ReadHitAnimationDefaults() {
    return state_->ReadHitAnimationDefaults();
}
ReadyAnimationDefaults OfficialCatalog::ReadReadyAnimationDefaults() {
    return state_->ReadReadyAnimationDefaults();
}
TextInput OfficialCatalog::ReadDefaultReadyTransition(std::int32_t from, std::int32_t to) {
    return state_->ReadDefaultReadyTransition(from, to);
}
PhaseVisualConfigSources OfficialCatalog::ReadPhaseVisualConfigSources(const std::string& character,
                                                                       const PhaseVisualSource& source,
                                                                       TypeRelationships& types) {
    return state_->ReadPhaseVisualConfigSources(character, source, types);
}
VisualConfigInput OfficialCatalog::ReadDefaultVisualConfig() {
    return state_->ReadDefaultVisualConfig();
}
VisualConfigInput OfficialCatalog::ReadCharacterVisualConfig(const std::string& path) {
    return state_->ReadCharacterVisualConfig(path);
}
VisualConfigSources OfficialCatalog::ReadVisualConfigSources(const std::string& path) {
    return state_->ReadVisualConfigSources(path);
}
ActionDataVersion OfficialCatalog::ReadActionDataVersion() {
    return state_->ReadActionDataVersion();
}
std::string OfficialCatalog::ReadCatalogRevision(const std::string& installed_revision) {
    return state_->ReadCatalogRevision(installed_revision);
}
std::unique_ptr<ProjectileTemplate> OfficialCatalog::ReadProjectileTemplate(const DefinitionSource& source,
                                                                            TypeRelationships& types) {
    return state_->ReadProjectileTemplate(source, types);
}
}
