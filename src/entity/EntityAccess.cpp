#include <hksr/entity/EntityAccess.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>

namespace hksr::sdk_v1 {
EntityAccess::EntityAccess(EntityAccessMethods methods):methods_(std::make_shared<const EntityAccessMethods>(std::move(methods))){}
EntityAccess::~EntityAccess()=default;
EntityAccess::EntityAccess(const EntityAccess&) noexcept=default;
EntityAccess& EntityAccess::operator=(const EntityAccess&) noexcept=default;

ModelAccess::ModelAccess(ModelAccessMethods methods):methods_(std::make_shared<const ModelAccessMethods>(std::move(methods))){}
ModelAccess::~ModelAccess()=default;
ModelAccess::ModelAccess(const ModelAccess&) noexcept=default;
ModelAccess& ModelAccess::operator=(const ModelAccess&) noexcept=default;

namespace {
template<class T, class... Args> Observed<T> ReadValue(Method method, Args... args) {
    Observed<T> result;
    result.read = native::Call(method, &result.value, args...);
    return result;
}
}
EntitySnapshot EntityAccess::Read(GameEntity* entity) const {
    if (!entity) return {};
    return {ReadValue<std::uint32_t>(methods_->runtime_id, entity),
        ReadValue<GameWorld*>(methods_->owner_world, entity), ReadValue<bool>(methods_->loaded, entity)};
}
Observed<GameEntity*> EntityAccess::Lookup(EntityManager* manager, std::uint32_t id) const {
    return manager ? ReadValue<GameEntity*>(methods_->manager_lookup, manager, id) : Observed<GameEntity*>{};
}
ObjectResult EntityAccess::Component(GameEntity* entity, void* type) const {
    ObjectResult result;
    if (entity && type) result.call = native::Call(methods_->component, &result.object, entity, type);
    return result;
}
RetirementResult EntityAccess::RequestRetirement(GameWorld* world, GameEntity* entity) const {
    RetirementResult result;
    // These are the two real dependencies of this operation, not all getters.
    if (!world || !entity || !methods_->kill_immediately.code || !methods_->make_die.code) return result;
    result.set_immediate = native::CallVoid(methods_->kill_immediately, entity, true);
    if (!result.set_immediate.returned) return result;
    result.request_attempted = true;
    result.request = native::CallVoid(methods_->make_die, world, entity, static_cast<GameEntity*>(nullptr));
    return result;
}
ModelSnapshot ModelAccess::Read(CharacterModelComponent* model) const {
    ModelSnapshot out;
    if (!model) return out;
    if (methods_->has_root_field)
        out.root.read.returned = native::CopyMemoryChecked(&out.root.value,
            reinterpret_cast<const unsigned char*>(model) + methods_->root_offset, sizeof(void*));
    out.art = Art(model);
    out.animator = ReadValue<void*>(methods_->animator, model);
    out.load_state = ReadValue<int>(methods_->load_state, model);
    out.changing = ReadValue<bool>(methods_->changing, model);
    out.root_loaded = ReadValue<bool>(methods_->root_loaded, model);
    out.art_loaded = ReadValue<bool>(methods_->art_loaded, model);
    return out;
}
Observed<void*> ModelAccess::Art(CharacterModelComponent* model) const {
    return model ? ReadValue<void*>(methods_->art,model) : Observed<void*>{};
}
}
