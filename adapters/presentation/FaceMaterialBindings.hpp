#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <functional>
namespace hksr::api::detail {
struct FaceMaterialBindings {
    void* face_type = nullptr;
    void* model_type = nullptr;
    void* entity_type = nullptr;
    Method art_loaded, on_art_loaded, component_owner;
    std::size_t cache_offset = 0;
};
FaceMaterialBindings ResolveFaceMaterialBindings(RuntimeMetadata&, const std::function<void*(ObjectResult)>& retain);
}
