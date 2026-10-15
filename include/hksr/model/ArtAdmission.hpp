#pragma once
#include <memory>
#include "../entity/EntityAccess.hpp"

namespace hksr::sdk_v1 {
struct ArtAdmissionMethods {
    Method supply,unload,hide_weapons;
    std::size_t logical_path_offset=0;
    bool has_logical_path=false;
    std::vector<std::string> binding_errors;
};
// Official model's logical Art slot is distinct from a prefab's resource path.
// Supplied GameObject must be an independently owned visual, never a live
// player or pooled model root. A returned call is NOT ownership/adoption proof;
// observe the model's Art and subsequent official lifecycle separately.
class ArtAdmission {
public:
    HKSR_API explicit ArtAdmission(ArtAdmissionMethods methods);
    HKSR_API ~ArtAdmission();
    HKSR_API ArtAdmission(const ArtAdmission&) noexcept;
    HKSR_API ArtAdmission& operator=(const ArtAdmission&) noexcept;
    HKSR_API ObjectResult LogicalPath(CharacterModelComponent* model) const;
    HKSR_API CallResult Supply(CharacterModelComponent* model,void* managed_key,void* managed_logical_path,
        bool option,void* callback,void* prepared_art) const;
    HKSR_API CallResult Unload(CharacterModelComponent* model,bool release_option) const;
    HKSR_API CallResult HideWeapons(CharacterModelComponent* model,void* managed_points) const;
private: std::shared_ptr<const ArtAdmissionMethods> methods_;
};
struct VisualObjectMethods {
    Method initialize,set_active,clone_under_parent,component_game_object,child_count;
    std::vector<std::string> binding_errors;
};
class VisualObjects {
public:
    HKSR_API explicit VisualObjects(VisualObjectMethods methods);
    HKSR_API ~VisualObjects();
    HKSR_API VisualObjects(const VisualObjects&) noexcept;
    HKSR_API VisualObjects& operator=(const VisualObjects&) noexcept;
    HKSR_API CallResult Initialize(void* allocated_game_object) const;
    HKSR_API CallResult SetActive(void* game_object,bool active) const;
    HKSR_API ObjectResult CloneUnderParent(void* source,void* parent_transform,bool world_position_stays) const;
    HKSR_API ObjectResult GameObject(void* component) const;
    HKSR_API Observed<int> ChildCount(void* transform) const;
private: std::shared_ptr<const VisualObjectMethods> methods_;
};
HKSR_API ArtAdmissionMethods ResolveArtAdmissionMethods(MethodResolver& source,const std::vector<DeclaredField>& model_fields);
HKSR_API VisualObjectMethods ResolveVisualObjectMethods(MethodResolver& source);
}
