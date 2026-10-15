#include <hksr/model/ArtAdmission.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
namespace hksr::sdk_v1 {
ArtAdmission::ArtAdmission(ArtAdmissionMethods methods):methods_(std::make_shared<const ArtAdmissionMethods>(std::move(methods))){}
ArtAdmission::~ArtAdmission()=default;
ArtAdmission::ArtAdmission(const ArtAdmission&) noexcept=default;
ArtAdmission& ArtAdmission::operator=(const ArtAdmission&) noexcept=default;

VisualObjects::VisualObjects(VisualObjectMethods methods):methods_(std::make_shared<const VisualObjectMethods>(std::move(methods))){}
VisualObjects::~VisualObjects()=default;
VisualObjects::VisualObjects(const VisualObjects&) noexcept=default;
VisualObjects& VisualObjects::operator=(const VisualObjects&) noexcept=default;

ObjectResult ArtAdmission::LogicalPath(CharacterModelComponent* model) const {
    ObjectResult out;
    if(model && methods_->has_logical_path) out.call.returned=native::CopyMemoryChecked(&out.object,
        reinterpret_cast<const unsigned char*>(model)+methods_->logical_path_offset,sizeof(void*));
    return out;
}
CallResult ArtAdmission::Supply(CharacterModelComponent* model,void* key,void* path,bool option,void* callback,void* art) const {
    return native::CallVoid(methods_->supply,model,key,path,option,callback,art);
}
CallResult ArtAdmission::Unload(CharacterModelComponent* model,bool option) const { return native::CallVoid(methods_->unload,model,option); }
CallResult ArtAdmission::HideWeapons(CharacterModelComponent* model,void* points) const { return native::CallVoid(methods_->hide_weapons,model,points); }
CallResult VisualObjects::Initialize(void* object) const { return native::CallVoid(methods_->initialize,object); }
CallResult VisualObjects::SetActive(void* object,bool active) const { return native::CallVoid(methods_->set_active,object,active); }
ObjectResult VisualObjects::CloneUnderParent(void* source,void* parent,bool world) const {
    ObjectResult out; out.call=native::Call(methods_->clone_under_parent,&out.object,source,parent,world); return out;
}
ObjectResult VisualObjects::GameObject(void* component) const {
    ObjectResult out; out.call=native::Call(methods_->component_game_object,&out.object,component); return out;
}
Observed<int> VisualObjects::ChildCount(void* transform) const {
    Observed<int> out; out.read=native::Call(methods_->child_count,&out.value,transform); return out;
}
}
