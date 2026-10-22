#include <hksr/presentation/audio/SoundAccess.hpp>
#include <hksr/presentation/audio/SoundCallbacks.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
SoundAccessMethods ResolveSoundAccess460(MethodResolver& source) {
    return {
        source.Resolve("RPG.Client","AudioManager","PostEventWithBeginCallback","System.UInt32",
            {"System.String","UnityEngine.GameObject","System.Action<System.UInt32,AkCallbackType,AkCallbackInfo>",
             "System.Action<System.UInt32>","AkCallbackType"}),
        source.Resolve("RPG.Client","AudioManager","StopEventID","System.Void",{"System.UInt32","System.Single"}),
        source.Resolve("UnityEngine","Object","IsNativeObjectAlive","System.Boolean",{"UnityEngine.Object"},MethodKind::Static)};
}
SoundDelegateLayout ResolveSoundDelegateLayout460(const std::vector<DeclaredField>& fields) {
    auto offset=[&](const char* name) {
        std::size_t value=0;unsigned count=0;
        for(const auto& field:fields)if(field.owner_namespace=="System" && field.owner_name=="Delegate" &&
            field.name==name && field.type=="System.IntPtr" && field.flags_observed && !(field.flags&0x10)) {
            value=field.offset;++count;
        }
        if(count!=1 || value<2*sizeof(void*) || value%alignof(void*))throw std::runtime_error("sound delegate IntPtr declaration unavailable");
        return value;
    };
    SoundDelegateLayout out{offset("method_ptr"),offset("invoke_impl"),offset("method_code")};
    if(out.method_ptr==out.invoke_impl || out.method_ptr==out.method_code || out.invoke_impl==out.method_code)
        throw std::runtime_error("sound delegate fields overlap");
    return out;
}
SoundCallbackAllocation ResolveSoundCallbacks460(RuntimeMetadata& metadata) {
    if(metadata.Version()!=RuntimeVersion::Hsr460)throw std::runtime_error("sound direct-Invoke ABI qualified for 4.6 only");
    const auto post=ResolveSoundAccess460(metadata).post;
    const auto began=metadata.ParameterType(post,3),ended=metadata.ParameterType(post,2);
    metadata.Resolve(began,"Invoke","System.Void",{"System.UInt32"});
    metadata.Resolve(ended,"Invoke","System.Void",{"System.UInt32","AkCallbackType","AkCallbackInfo"});
    auto allocation=[&](RuntimeType type) {
        return SoundDelegateAllocation{[&metadata,type]{return metadata.AllocateObject(type);},
            ResolveSoundDelegateLayout460(metadata.ReadFields(type))};
    };
    return {allocation(began),allocation(ended)};
}
}
