#pragma once
#include <memory>
#include "../managed/References.hpp"
#include <string>
#include <vector>
#include <utility>

namespace hksr::sdk_v1 {
struct SceneObjectMethods {
    Method transform, components, name, string_equals;
    std::vector<std::string> binding_errors;
};
// Borrowed objects, scoped to an already-owned model. No scene-wide search and
// no assumptions about the managed String memory layout or character set.
class SceneObjects {
public:
    HKSR_API explicit SceneObjects(SceneObjectMethods methods);
    HKSR_API ~SceneObjects();
    HKSR_API SceneObjects(const SceneObjects&) noexcept;
    HKSR_API SceneObjects& operator=(const SceneObjects&) noexcept;
    HKSR_API ObjectResult Transform(void* game_object) const;
    HKSR_API ObjectResult Components(void* game_object,void* component_system_type,bool include_inactive) const;
    HKSR_API ObjectResult Name(void* unity_object) const;
    HKSR_API Observed<bool> EqualNames(void* first_managed_string,void* second_managed_string) const;
private: std::shared_ptr<const SceneObjectMethods> methods_;
};
HKSR_API SceneObjectMethods ResolveSceneObjectMethods(MethodResolver& source);
}
