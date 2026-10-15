#pragma once
#include <memory>
#include "../managed/References.hpp"

namespace hksr::sdk_v1 {
struct RendererMethods { Method get_mesh, set_mesh, get_materials, set_materials, get_bones, set_bones; };
// Changes references on a renderer, not shared asset contents. Does not create
// or destroy meshes/materials, perform role matching, or claim ownership.
class RendererAccess {
public:
    HKSR_API explicit RendererAccess(RendererMethods methods);
    HKSR_API ~RendererAccess();
    HKSR_API RendererAccess(const RendererAccess&) noexcept;
    HKSR_API RendererAccess& operator=(const RendererAccess&) noexcept;
    HKSR_API ObjectResult Mesh(void* skinned_renderer) const;
    HKSR_API ObjectResult Materials(void* renderer) const;
    HKSR_API CallResult SetMesh(void* skinned_renderer, void* mesh) const;
    HKSR_API CallResult SetMaterials(void* renderer, void* material_array) const;
    HKSR_API ObjectResult Bones(void* skinned_renderer) const;
    HKSR_API CallResult SetBones(void* skinned_renderer, void* transform_array) const;
private:
    std::shared_ptr<const RendererMethods> methods_;
};
HKSR_API RendererMethods ResolveRendererMethods(MethodResolver& resolver);
}
