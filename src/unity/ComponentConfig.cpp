#include <hksr/unity/ComponentConfig.hpp>
#include <hksr/native/Invocation.hpp>
#include <Windows.h>

namespace hksr::sdk_v1 {
ComponentConfig::ComponentConfig(ComponentConfigMethods methods):methods_(std::make_shared<const ComponentConfigMethods>(std::move(methods))){}
ComponentConfig::~ComponentConfig()=default;
ComponentConfig::ComponentConfig(const ComponentConfig&) noexcept=default;
ComponentConfig& ComponentConfig::operator=(const ComponentConfig&) noexcept=default;

ObjectResult ComponentConfig::Serialize(void* value) const {
    ObjectResult out; if(value) out.call=native::Call(methods_->serialize,&out.object,value,false); return out;
}
CallResult ComponentConfig::Overwrite(void* text,void* value) const {
    if(!text || !value) return {}; return native::CallVoid(methods_->overwrite,text,value);
}
ObjectResult ComponentConfig::Add(void* go,void* type) const {
    ObjectResult out; if(go && type) out.call=native::Call(methods_->add,&out.object,go,type); return out;
}
ObjectResult ComponentConfig::Type(void* value) const {
    ObjectResult out; if(value) out.call=native::Call(methods_->type,&out.object,value); return out;
}
Observed<std::int32_t> ComponentConfig::InstanceId(void* value) const {
    Observed<std::int32_t> out; if(value) out.read=native::Call(methods_->instance_id,&out.value,value); return out;
}
ObjectResult ComponentConfig::Parent(void* value) const {
    ObjectResult out; if(value) out.call=native::Call(methods_->parent,&out.object,value); return out;
}
ObjectResult ComponentConfig::Transform(void* value) const {
    ObjectResult out; if(value) out.call=native::Call(methods_->transform,&out.object,value); return out;
}
TextResult ComponentConfig::ReadText(void* value) const {
    TextResult out; if(!value) return out;
    int count=0; out.read=native::Call(methods_->string_length,&count,value);
    if(!out.read.returned || count<0) { out.read.returned=false; return out; }
    std::wstring text(static_cast<std::size_t>(count),L'\0');
    for(int i=0;i<count;++i) {
        std::uint16_t character=0; out.read=native::Call(methods_->string_character,&character,value,i);
        if(!out.read.returned) return out;
        text[static_cast<std::size_t>(i)]=static_cast<wchar_t>(character);
    }
    if(!count) return out;
    const int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),count,nullptr,0,nullptr,nullptr);
    if(!bytes) { out.read.returned=false; out.error="invalid UTF-16 string"; return out; }
    out.utf8.resize(static_cast<std::size_t>(bytes));
    if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),count,out.utf8.data(),bytes,nullptr,nullptr)!=bytes) {
        out.read.returned=false; out.error="UTF-8 conversion failed"; out.utf8.clear();
    }
    return out;
}
}
