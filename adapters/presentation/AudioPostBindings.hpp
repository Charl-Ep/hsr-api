#pragma once
#include "../../src/presentation/audio/AudioPostRoute.hpp"
#include <hksr/metadata/RuntimeMetadata.hpp>
namespace hksr::api::detail {
AudioPostOperations ResolveAudioPostOperations(RuntimeMetadata&, ReferenceMethods);
std::function<void*()> BindAnimationAudioEmitter(ObjectMetadata&, void* event_component);
std::function<void*()> BindAudioManagerCallback(ObjectMetadata&, void* manager);
template <class Body> struct AudioPostThunk {
    static std::uint32_t __fastcall Invoke(void* name, void* emitter, std::uint32_t flags, void* callback, void* cookie,
                                           void* metadata) {
        return Body::Invoke(AudioPostArguments{name, emitter, callback, cookie, metadata, flags});
    }
    static void* Address() {
        return reinterpret_cast<void*>(&Invoke);
    }
};
inline std::uint32_t ForwardAudioPost(void* original, const AudioPostArguments& args) {
    return reinterpret_cast<std::uint32_t(__fastcall*)(void*, void*, std::uint32_t, void*, void*, void*)>(original)(
        args.event_name, args.emitter, args.flags, args.callback, args.cookie, args.metadata);
}
}
