#include <hksr/presentation/events/AnimationEvents.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
namespace hksr::sdk_v1 {
AnimationEvents::AnimationEvents(AnimationEventMethods methods):methods_(std::make_shared<const AnimationEventMethods>(std::move(methods))){}
AnimationEvents::~AnimationEvents()=default;
AnimationEvents::AnimationEvents(const AnimationEvents&) noexcept=default;
AnimationEvents& AnimationEvents::operator=(const AnimationEvents&) noexcept=default;

ObjectResult AnimationEvents::Context(void* component) const {
    ObjectResult out; if(component) out.call=native::Call(methods_->context,&out.object,component); return out;
}
ObjectResult AnimationEvents::Caster(void* context) const {
    ObjectResult out; if(context) out.call=native::Call(methods_->caster,&out.object,context); return out;
}
ObjectResult AnimationEvents::Records(void* component) const {
    ObjectResult out; if(component) out.call=native::Call(methods_->records,&out.object,component); return out;
}
ObjectResult AnimationEvents::Consumer(void* component) const {
    ObjectResult out;
    if(component && methods_->has_consumer) out.call.returned=native::CopyMemoryChecked(
        &out.object,static_cast<const unsigned char*>(component)+methods_->consumer_offset,sizeof(void*));
    return out;
}
ObjectResult AnimationEvents::Register(void* component,void* context,void* path) const {
    ObjectResult out;
    if(component && context && path) out.call=native::Call(methods_->register_file,&out.object,component,context,path);
    return out;
}
CallResult AnimationEvents::Detach(void* component,void* record,bool option) const {
    if(!component || !record) return {};
    return native::CallVoid(methods_->detach,component,record,option);
}
ObjectResult AnimationEvents::RegisterInline(void* component,void* context,void* events,void* behaviors,void* groups) const {
    ObjectResult out;
    if(component && context && (events || behaviors || groups))
        out.call=native::Call(methods_->register_inline,&out.object,component,context,events,behaviors,groups);
    return out;
}
CallResult AnimationEvents::Dispose(void* record) const {
    if(!record) return {};
    return native::CallVoid(methods_->dispose_record,record);
}
}
