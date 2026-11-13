#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/managed/ManagedLists.hpp>
#include <hksr/managed/GenericFactory.hpp>
#include <functional>
#include <optional>
namespace hksr::api::detail {
struct ModelPresentationBindings {
    void* model_type = nullptr;
    void* face_type = nullptr;
    void* follow_type = nullptr;
    void* group_type = nullptr;
    Method face_set, scale_set, scale_read, follow_initialize, follow_set, component_owner, group_members;
    std::optional<ManagedReferenceList> members;
    std::size_t cheek = 0, shy = 0, shadow = 0, face_cache = 0, follow_target = 0, follow_point = 0,
                follow_multiplier = 0, follow_late = 0, follow_outline = 0, follow_model = 0;
    std::string face_error, scale_error, follow_error;
};
using ModelPresentationType = std::function<void*(const char*, const char*)>;
using ModelPresentationFollowFactory = std::function<void(void*, void*)>;
ModelPresentationBindings ResolveModelPresentationBindings(RuntimeMetadata&, const ModelPresentationType&,
                                                           const ModelPresentationFollowFactory&);
bool PrepareModelFollowFactory(GenericFactory&, void* owner_type, void* follow_type);
}
