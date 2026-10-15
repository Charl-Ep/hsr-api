#include <hksr/model/ProfileInputs.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>

namespace hksr::sdk_v1 {
ProfileInputs::ProfileInputs(ProfileInputMethods methods):methods_(std::make_shared<const ProfileInputMethods>(std::move(methods))){}
ProfileInputs::~ProfileInputs()=default;
ProfileInputs::ProfileInputs(const ProfileInputs&) noexcept=default;
ProfileInputs& ProfileInputs::operator=(const ProfileInputs&) noexcept=default;

namespace {
ObjectResult Read(Method method,void* object) {
    ObjectResult out;
    if(object) out.call=native::Call(method,&out.object,object);
    return out;
}
ObjectResult Field(void* object,std::size_t offset,bool available) {
    ObjectResult out;
    if(object && available) out.call.returned=native::CopyMemoryChecked(&out.object,
        static_cast<const unsigned char*>(object)+offset,sizeof(out.object));
    return out;
}
}
ObjectResult ProfileInputs::ModelPath(void* model) const { return Read(methods_->model_path,model); }
ObjectResult ProfileInputs::EntityConfig(void* model) const { return Read(methods_->entity_config,model); }
CallResult ProfileInputs::InitializeOperation(void* operation) const {
    return operation?native::CallVoid(methods_->operation_constructor,operation):CallResult{};
}
Observed<bool> ProfileInputs::AnimatorInitialized(void* animator) const {
    Observed<bool> out;
    if(animator) out.read=native::Call(methods_->animator_initialized,&out.value,animator);
    return out;
}
ProfileEventArrays ReadProfileEventArrays(void* consumer,ProfileEventFields fields) {
    return {Field(consumer,fields.paths,fields.has_paths),Field(consumer,fields.layers,fields.has_layers)};
}
}
