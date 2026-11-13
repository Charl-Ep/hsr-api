#include <hksr/model/ModelAccess.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <utility>

namespace hksr::api {
ModelAccess::ModelAccess(ModelAccessMethods methods)
    : methods_(std::make_shared<const ModelAccessMethods>(std::move(methods))) {}
ModelAccess::~ModelAccess() = default;
ModelAccess::ModelAccess(const ModelAccess&) noexcept = default;
ModelAccess& ModelAccess::operator=(const ModelAccess&) noexcept = default;

namespace {
template <class T> Observed<T> ReadValue(Method method, CharacterModelComponent* model) {
    Observed<T> result;
    result.read = native::Call(method, &result.value, model);
    return result;
}
}
ModelSnapshot ModelAccess::Read(CharacterModelComponent* model) const {
    ModelSnapshot out;
    if (!model)
        return out;
    if (methods_->has_root_field)
        out.root.read.returned = native::CopyMemoryChecked(
            &out.root.value, reinterpret_cast<const unsigned char*>(model) + methods_->root_offset, sizeof(void*));
    out.art = Art(model);
    out.animator = ReadValue<void*>(methods_->animator, model);
    out.load_state = ReadValue<int>(methods_->load_state, model);
    out.changing = ReadValue<bool>(methods_->changing, model);
    out.root_loaded = ReadValue<bool>(methods_->root_loaded, model);
    out.art_loaded = ReadValue<bool>(methods_->art_loaded, model);
    return out;
}
Observed<void*> ModelAccess::Art(CharacterModelComponent* model) const {
    return model ? ReadValue<void*>(methods_->art, model) : Observed<void*>{};
}
}
