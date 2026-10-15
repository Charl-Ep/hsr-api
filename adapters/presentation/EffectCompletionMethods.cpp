#include <hksr/presentation/effects/EffectCompletion.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
EffectCompletionLayout ResolveEffectCompletionLayout460(const std::vector<DeclaredField>& fields) {
    auto offset=[&](const char* name) {
        unsigned count=0;std::size_t value=0;
        for(const auto& field:fields)if(field.owner_namespace=="System" && field.owner_name=="Delegate" &&
            field.name==name && field.type=="System.IntPtr" && field.flags_observed && !(field.flags&0x10)) {
            ++count;value=field.offset;
        }
        if(count!=1 || value<2*sizeof(void*) || value%alignof(void*))throw std::runtime_error("effect delegate IntPtr declaration unavailable");
        return value;
    };
    EffectCompletionLayout out{offset("method_ptr"),offset("invoke_impl"),offset("method_code")};
    if(out.method_ptr==out.invoke_impl || out.method_ptr==out.method_code || out.invoke_impl==out.method_code)
        throw std::runtime_error("effect delegate fields overlap");
    return out;
}
EffectCompletionAllocation ResolveEffectCompletion460(RuntimeMetadata& metadata) {
    if(metadata.Version()!=RuntimeVersion::Hsr460)throw std::runtime_error("effect callback direct-Invoke ABI qualified for 4.6 only");
    const auto setter=metadata.Resolve("RPG.Client","TriggerEffectParams","InnerSetReleaseCallback","System.Void",
        {"System.Action<RPG.Client.MonoEffect,RPG.Client.TriggerEffectCallbackParams>"});
    const auto type=metadata.ParameterType(setter,0);
    // Existence of this exact closed signature, not the open generic definition.
    metadata.Resolve(type,"Invoke","System.Void",{"RPG.Client.MonoEffect","RPG.Client.TriggerEffectCallbackParams"});
    if(!metadata.TypeDerivesFrom("RPG.Client","TriggerEffectCallbackParams","System","ValueType"))
        throw std::runtime_error("effect callback payload no longer a value type");
    unsigned payload_fields=0;
    for(const auto& field:metadata.ReadFields("RPG.Client","TriggerEffectCallbackParams")) {
        if(!field.flags_observed)throw std::runtime_error("effect callback payload field flags unavailable");
        if(field.flags&0x10)continue;
        ++payload_fields;
        if(field.type!="RPG.Client.TriggerEffectParams" || field.offset!=2*sizeof(void*))
            throw std::runtime_error("effect callback payload is not the qualified single-reference value");
    }
    if(payload_fields!=1)throw std::runtime_error("effect callback payload field count changed");
    const auto layout=ResolveEffectCompletionLayout460(metadata.ReadFields(type));
    return {[&metadata,type]{return metadata.AllocateObject(type);},layout};
}
}
