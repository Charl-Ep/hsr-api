#include <hksr/presentation/effects/EffectAccess.hpp>
#include <hksr/native/Invocation.hpp>

namespace hksr::sdk_v1 {
EffectAccess::EffectAccess(EffectAccessMethods methods):methods_(std::make_shared<const EffectAccessMethods>(std::move(methods))){}
EffectAccess::~EffectAccess()=default;
EffectAccess::EffectAccess(const EffectAccess&) noexcept=default;
EffectAccess& EffectAccess::operator=(const EffectAccess&) noexcept=default;

ObjectResult EffectAccess::MapPath(void* entity,void* path) const {
    ObjectResult out;if(entity && path)out.call=native::Call(methods_->map_path,&out.object,entity,path);return out;
}
EffectAcquireResult EffectAccess::Acquire(void* path) const {
    EffectAcquireResult out;if(!path)return out;
    bool empty=false;out.call=native::Call(methods_->path_empty,&empty,path);
    if(!out.call.returned || empty)return out;
    out.call=native::Call(methods_->acquire,&out.reference,path);return out;
}
EffectObservation EffectAccess::Observe(EffectReference reference) const {
    EffectObservation out;auto* input=reference.parameters;if(!input)return out;
    out.id_before.read=native::Call(methods_->id,&out.id_before.value,input);
    out.released.read=native::Call(methods_->released,&out.released.value,input);
    out.state.read=native::Call(methods_->state,&out.state.value,input);
    out.instance.call=native::Call(methods_->instance,&out.instance.object,input);
    out.id_after.read=native::Call(methods_->id,&out.id_after.value,input);
    out.same_generation=out.id_before.read.returned && out.id_after.read.returned &&
        out.id_before.value==reference.id && out.id_after.value==reference.id;
    return out;
}
CallResult EffectAccess::Submit(void* manager,EffectReference reference) const {
    if(!manager || !reference.parameters)return {};
    const auto observed=Observe(reference);
    if(!observed.same_generation || !observed.released.read.returned || observed.released.value)return {};
    // May synchronously call completion, including the synchronous FireEffect
    // branch. The request owner must install its callback and ledger FIRST.
    return native::CallVoid(methods_->submit,manager,reference.parameters);
}
Observed<bool> EffectAccess::Remove(void* manager,EffectReference reference,bool fade) const {
    Observed<bool> out;
    // RemoveEffectByTriggerParamsRef eventually sets only the numeric filter;
    // the manager interprets zero as an unrestricted ID.
    if(manager && reference.parameters && reference.id)out.read=native::Call(methods_->remove,&out.value,manager,reference,fade);
    return out;
}
CallResult EffectAccess::ReleaseUnsubmitted(EffectReference reference) const {
    if(!reference.parameters)return {};
    const auto observed=Observe(reference);
    if(!observed.same_generation || !observed.released.read.returned)return {};
    if(observed.released.value)return {true,0};
    return native::CallVoid(methods_->release,reference.parameters);
}
}
