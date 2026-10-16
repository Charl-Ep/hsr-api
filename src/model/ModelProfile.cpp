#include <hksr/model/ModelProfile.hpp>
#include <hksr/native/Invocation.hpp>

namespace hksr::sdk_v1 {
struct ModelProfile::State:ModelMethods {
    Method pending;
    State(ModelMethods value,Method read):ModelMethods(std::move(value)),pending(read){}
};
ModelProfile::ModelProfile(ModelMethods methods):ModelProfile(std::move(methods),{}){}
ModelProfile::ModelProfile(ModelMethods methods,Method pending):methods_(std::make_shared<const State>(std::move(methods),pending)){}
ModelProfile::~ModelProfile()=default;
ModelProfile::ModelProfile(const ModelProfile&) noexcept=default;
ModelProfile& ModelProfile::operator=(const ModelProfile&) noexcept=default;

CallResult ModelProfile::Begin(void* operation,void* entity,ModelInputs inputs,bool asynchronous,void* callback) const {
    return native::CallVoid(methods_->begin,operation,entity,inputs,asynchronous,callback);
}
CallResult ModelProfile::Cancel(void* operation) const {
    return native::CallVoid(methods_->cancel,operation);
}
bool ModelProfile::HasAsyncCompletion() const {return methods_->pending.code!=nullptr;}
Observed<bool> ModelProfile::Pending(void* operation) const {
    Observed<bool> out;
    if(operation)out.read=native::Call(methods_->pending,&out.value,operation);
    return out;
}
ChangeResult ModelProfile::Change(void* model,void* path,void* root) const {
    ChangeResult result;result.call=native::Call(methods_->change,&result.changed,model,path,root);return result;
}
ChangeResult ModelProfile::CanChange(void* model,void* path,void** resolved_path) const {
    ChangeResult result;result.call=native::Call(methods_->can_change,&result.changed,model,path,resolved_path);return result;
}
CallResult ModelProfile::ReplaceEvents(void* component,void* paths,void* layers) const {
    return native::CallVoid(methods_->events,component,paths,layers);
}
CallResult ModelProfile::Restore(void* model,void* root_path,void* entity_config) const {
    // Root prefab and entity configuration are distinct inputs. Cancellation
    // does not restore either of them; caller explicitly chooses to restore.
    return native::CallVoid(methods_->restore,model,root_path,entity_config,static_cast<void*>(nullptr));
}
}
