#include <hksr/unity/RendererAccess.hpp>
#include <hksr/native/Invocation.hpp>
namespace hksr::sdk_v1 {
RendererAccess::RendererAccess(RendererMethods methods):methods_(std::make_shared<const RendererMethods>(std::move(methods))){}
RendererAccess::~RendererAccess()=default;
RendererAccess::RendererAccess(const RendererAccess&) noexcept=default;
RendererAccess& RendererAccess::operator=(const RendererAccess&) noexcept=default;

ObjectResult RendererAccess::Mesh(void* renderer) const {
    ObjectResult result; result.call = native::Call(methods_->get_mesh, &result.object, renderer); return result;
}
ObjectResult RendererAccess::Materials(void* renderer) const {
    ObjectResult result; result.call = native::Call(methods_->get_materials, &result.object, renderer); return result;
}
CallResult RendererAccess::SetMesh(void* renderer, void* mesh) const { return native::CallVoid(methods_->set_mesh, renderer, mesh); }
CallResult RendererAccess::SetMaterials(void* renderer, void* array) const { return native::CallVoid(methods_->set_materials, renderer, array); }
ObjectResult RendererAccess::Bones(void* renderer) const {
    ObjectResult result; result.call=native::Call(methods_->get_bones,&result.object,renderer); return result;
}
CallResult RendererAccess::SetBones(void* renderer,void* array) const { return native::CallVoid(methods_->set_bones,renderer,array); }
}
