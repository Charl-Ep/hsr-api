#include <hksr/animation/AdventureConfig.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
namespace hksr::sdk_v1 {
AdventureConfig::AdventureConfig(AdventureConfigMethods methods):methods_(std::make_shared<const AdventureConfigMethods>(std::move(methods))){}
AdventureConfig::~AdventureConfig()=default;
AdventureConfig::AdventureConfig(const AdventureConfig&) noexcept=default;
AdventureConfig& AdventureConfig::operator=(const AdventureConfig&) noexcept=default;

ObjectResult ReadAdventureDataConfig(void* component,const AdventureDataField& field) {
    ObjectResult out;
    if(component && field.available) out.call.returned=native::CopyMemoryChecked(
        &out.object,static_cast<const unsigned char*>(component)+field.offset,sizeof(void*));
    return out;
}
ObjectResult AdventureConfig::Load(void* path) const {
    ObjectResult out;
    if(path) out.call=native::Call(methods_->load,&out.object,path);
    return out;
}
AdventureConfigSnapshot AdventureConfig::Read(void* config) const {
    AdventureConfigSnapshot out;
    if(!config) return out;
    if(methods_->has_hidden_weapons) out.hidden_weapons.call.returned=native::CopyMemoryChecked(
        &out.hidden_weapons.object,static_cast<const unsigned char*>(config)+methods_->hidden_weapons_offset,sizeof(void*));
    if(methods_->has_event_paths) out.event_paths.call.returned=native::CopyMemoryChecked(
        &out.event_paths.object,static_cast<const unsigned char*>(config)+methods_->event_paths_offset,sizeof(void*));
    return out;
}
}
