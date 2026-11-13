#include "AudioPostBindings.hpp"
#include "../../src/metadata/InstanceField.hpp"
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <stdexcept>
namespace hksr::api::detail {
std::function<void*()> BindAudioManagerCallback(ObjectMetadata& metadata, void* manager) {
    const auto field =
        ResolveInstanceField(metadata, manager, "_PostCallBackAction", "AkCallbackManager.EventCallback");
    return [manager, offset = field.offset] {
        void* value = nullptr;
        if (!native::CopyMemoryChecked(&value, static_cast<char*>(manager) + offset, sizeof(value)))
            throw std::runtime_error("audio manager callback unreadable");
        return value;
    };
}
std::function<void*()> BindAnimationAudioEmitter(ObjectMetadata& metadata, void* component) {
    const auto field = ResolveInstanceField(metadata, component, "AAPEMMDGOJO", "UnityEngine.GameObject");
    return [component, offset = field.offset] {
        void* value = nullptr;
        if (!native::CopyMemoryChecked(&value, static_cast<char*>(component) + offset, sizeof(value)))
            throw std::runtime_error("animation audio emitter unreadable");
        return value;
    };
}
AudioPostOperations ResolveAudioPostOperations(RuntimeMetadata& m, ReferenceMethods references) {
    AudioPostOperations out;
    out.references = references;
    out.entry = m.Resolve("", "AkSoundEngine", "PostEvent", "System.UInt32",
                          {"System.String", "UnityEngine.GameObject", "System.UInt32",
                           "AkCallbackManager.EventCallback", "System.Object"},
                          MethodKind::Static);
    const auto open =
        m.Resolve("", "AkSoundEngine", "DynamicSequenceOpen", "System.UInt32",
                  {"UnityEngine.GameObject", "System.UInt32", "AkCallbackManager.EventCallback", "System.Object"},
                  MethodKind::Static);
    out.open = [open](const AudioPostArguments& a) {
        AudioPostOpen value;
        value.call = native::Call(open, &value.id, a.emitter, a.flags, a.callback, a.cookie);
        return value;
    };
    const auto lock = m.Resolve("", "AkSoundEngine", "DynamicSequenceLockPlaylist", "AkPlaylist", {"System.UInt32"},
                                MethodKind::Static);
    out.lock = [lock](std::uint32_t id) {
        ObjectResult value;
        value.call = native::Call(lock, &value.object, id);
        return value;
    };
    const auto enqueue = m.Resolve("", "AkPlaylist", "Enqueue", "AKRESULT", {"System.UInt32"});
    out.enqueue = [enqueue](void* playlist, std::uint32_t node) {
        std::uint32_t code = 0;
        AudioPostOperation value;
        value.call = native::Call(enqueue, &code, playlist, node);
        value.native_code = code;
        value.accepted = value.call.returned && code == 1;
        return value;
    };
    const auto operation = [&m](const char* name) {
        const auto method = m.Resolve("", "AkSoundEngine", name, "AKRESULT", {"System.UInt32"}, MethodKind::Static);
        return [method](std::uint32_t id) {
            std::uint32_t code = 0;
            AudioPostOperation value;
            value.call = native::Call(method, &code, id);
            value.native_code = code;
            value.accepted = value.call.returned && code == 1;
            return value;
        };
    };
    out.unlock = operation("DynamicSequenceUnlockPlaylist");
    out.play = operation("DynamicSequencePlay");
    const auto stop = m.Resolve("", "AkSoundEngine", "ExecuteActionOnPlayingID", "System.Void",
                                {"AkActionOnEventType", "System.UInt32", "System.Int32"}, MethodKind::Static);
    out.stop = [stop](std::uint32_t id) {
        AudioPostOperation value;
        value.call = native::CallVoid(stop, std::uint32_t{0}, id, std::int32_t{0});
        value.accepted = value.call.returned;
        return value;
    };
    out.close = operation("DynamicSequenceClose");
    return out;
}
}
