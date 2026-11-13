#include <hksr/unity/SceneObjects.hpp>
#include <hksr/native/Invocation.hpp>
namespace hksr::api {
SceneObjects::SceneObjects(SceneObjectMethods methods)
    : methods_(std::make_shared<const SceneObjectMethods>(std::move(methods))) {}
SceneObjects::~SceneObjects() = default;
SceneObjects::SceneObjects(const SceneObjects&) noexcept = default;
SceneObjects& SceneObjects::operator=(const SceneObjects&) noexcept = default;

ObjectResult SceneObjects::Transform(void* object) const {
    ObjectResult r;
    r.call = native::Call(methods_->transform, &r.object, object);
    return r;
}
ObjectResult SceneObjects::Components(void* object, void* type, bool inactive) const {
    ObjectResult r;
    r.call = native::Call(methods_->components, &r.object, object, type, inactive);
    return r;
}
ObjectResult SceneObjects::Name(void* object) const {
    ObjectResult r;
    r.call = native::Call(methods_->name, &r.object, object);
    return r;
}
Observed<bool> SceneObjects::EqualNames(void* first, void* second) const {
    Observed<bool> r;
    r.read = native::Call(methods_->string_equals, &r.value, first, second);
    return r;
}
HierarchyAccess::HierarchyAccess(HierarchyMethods methods)
    : methods_(std::make_shared<const HierarchyMethods>(std::move(methods))) {}
HierarchyAccess::~HierarchyAccess() = default;
HierarchyAccess::HierarchyAccess(const HierarchyAccess&) noexcept = default;
HierarchyAccess& HierarchyAccess::operator=(const HierarchyAccess&) noexcept = default;

ObjectResult HierarchyAccess::Find(void* root, void* path) const {
    ObjectResult r;
    r.call = native::Call(methods_->find, &r.object, root, path);
    return r;
}
}
