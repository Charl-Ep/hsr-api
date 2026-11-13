#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/managed/ManagedLists.hpp>
#include <functional>
#include <optional>
#include <string>

namespace hksr::api::detail {
struct RenderingBindings {
    void *model_type = nullptr, *renderer_type = nullptr, *mapping_type = nullptr, *effect_type = nullptr,
         *visible_type = nullptr;
    RuntimeType list_type;
    std::optional<ManagedReferenceList> list;
    Method component, root_component, component_transform, parent, attachment_game_object, outline_game_object, alive,
        mask_read, mask_write;
    Method find_attach, shader_property, find_renderer, outline_set, visibility_set, list_ctor, list_add;
    std::size_t outline_add = 0, outline_remove = 0, renderer_cache = 0;
    std::string model_error, renderer_error, mask_read_error, mask_write_error, attachment_error, outline_error,
        outline_write_error, named_error, visibility_error;
};
using RenderingType = std::function<void*(const char*, const char*)>;
using RenderingFields = std::function<std::vector<DeclaredField>(const char*, const char*, bool)>;
using RenderingOutlineList = std::function<void(RenderingBindings&)>;
std::size_t ResolveRenderingRendererCacheOffset(const RenderingFields&);
RenderingBindings ResolveRenderingBindings(MethodResolver&, const RenderingFields&, const RenderingType&,
                                           const RenderingOutlineList&, const RenderingOutlineList& write_list = {});
RenderingBindings ResolveRenderingBindings(RuntimeMetadata&, const RenderingType&);
}
