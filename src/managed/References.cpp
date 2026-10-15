#include <hksr/managed/References.hpp>
#include <hksr/native/Invocation.hpp>

namespace hksr::sdk_v1 {
References::References(ReferenceMethods methods):methods_(std::make_shared<const ReferenceMethods>(std::move(methods))){}
References::~References()=default;
References::References(const References&) noexcept=default;
References& References::operator=(const References&) noexcept=default;

RootResult References::Retain(void* object) const {
    RootResult result;
    // Locked contract used by the former runtime: new handle0, GCHandleType2.
    result.call=native::Call(methods_->retain,&result.handle,object,0,2);return result;
}
ObjectResult References::Target(std::int32_t handle) const {
    ObjectResult result;result.call=native::Call(methods_->get,&result.object,handle);return result;
}
RootResult References::Watch(void* object) const {
    RootResult result;
    // HSR 4.6's IL2CPP explicitly throws for WeakTrackResurrection (1).
    // Ordinary Weak (0) uses the supported non-owning allocation branch.
    result.call=native::Call(methods_->retain,&result.handle,object,0,0);return result;
}
CallResult References::Free(std::int32_t handle) const {return native::CallVoid(methods_->free,handle);}
ObjectResult References::LoadAsset(void* path,void* type,bool option) const {
    ObjectResult result;result.call=native::Call(methods_->load_asset,&result.object,path,type,option);return result;
}
CallResult References::ReleaseAsset(void* object) const {return native::CallVoid(methods_->release_asset,object);}
}
