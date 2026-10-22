#include "SoundManager.hpp"
#include <hksr/native/Invocation.hpp>
#include <stdexcept>

namespace hksr::sdk_v1::detail {
SoundManager::SoundManager(RuntimeMetadata& metadata,ReferenceMethods references)
    :metadata_(metadata),services_{references,SoundAccess(ResolveSoundAccess460(metadata)),ResolveSoundCallbacks460(metadata),{},ResolveSoundTracking460(metadata)},roots_(references){}
SoundRequestServices SoundManager::Services() const{return services_;}
void* SoundManager::Keep(ObjectResult value) {
    if(!value.call.returned || !value.object)throw std::runtime_error("audio metadata object unavailable");
    held_.reserve(held_.size()+1);
    const auto id=roots_.Keep(value.object);held_.push_back(id);
    if(!roots_.Status(id).ready)throw std::runtime_error("audio metadata retention failed");
    return value.object;
}
ObjectResult SoundManager::Manager() {
    if(closed_)return {};
    if(!field_) {
        auto* type=Keep(metadata_.ManagedType("RPG.Client","GlobalVars"));
        auto* name=Keep(metadata_.NewStringUtf8("s_AudioManager"));
        const auto find=metadata_.Resolve("System","RuntimeType","GetField","System.Reflection.FieldInfo",
            {"System.String","System.Reflection.BindingFlags"});
        ObjectResult found;found.call=native::Call(find,&found.object,type,name,56);
        auto* field=Keep(found);
        const auto actual=metadata_.ReadObjectType(field);MetadataResolver resolver({actual});
        get_=resolver.Resolve(actual.name_space.c_str(),actual.name.c_str(),"GetValue","System.Object",{"System.Object"});
        field_=field;
    }
    ObjectResult out;out.call=native::Call(get_,&out.object,field_,static_cast<void*>(nullptr));return out;
}
bool SoundManager::Close() {
    closed_=true;field_=nullptr;
    for(auto it=held_.begin();it!=held_.end();) {
        if(roots_.Release(*it).complete){roots_.Forget(*it);it=held_.erase(it);}else ++it;
    }
    return held_.empty();
}
}
