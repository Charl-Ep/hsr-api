#pragma once
#include <hksr/presentation/audio/SoundCallbacks.hpp>
#include <hksr/presentation/effects/EffectCompletion.hpp>
#include <hksr/native/MetadataReader.hpp>

namespace hksr::api::detail {
template <auto Sink> void __fastcall SoundBeganBridge(void* cookie, std::uint32_t id, void*) noexcept {
    Sink(cookie, SoundCallbackEvent{SoundCallbackKind::Began, id, false});
}
template <auto Sink>
void __fastcall SoundEndedBridge(void* cookie, std::uint32_t id, std::uint32_t flags, void* info, void*) noexcept {
    if (flags == 1)
        Sink(cookie, SoundCallbackEvent{SoundCallbackKind::Ended, id, info != nullptr});
}
template <auto Sink>
void __fastcall EffectCompletionBridge(void* cookie, void* effect, void* parameters, void*) noexcept {
    Sink(cookie, EffectCompletionEvent{effect, parameters});
}
template <class Layout>
bool BindCallbackDelegate(void* object, const Layout& layout, void* entry, std::uintptr_t cookie) {
    const auto write = [&](std::size_t offset, void* value) {
        return native::CopyMemoryChecked(static_cast<unsigned char*>(object) + offset, &value, sizeof(value));
    };
    return write(layout.method_ptr, entry) && write(layout.invoke_impl, entry) &&
           write(layout.method_code, reinterpret_cast<void*>(cookie));
}
}
