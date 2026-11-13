#pragma once
#include <cstdint>
namespace hksr::api::detail {
inline void* ForwardTimelineBorrow(void* entry, void* self, void* name, void* resource, void* asset,
                                   const void* position, const void* rotation, void* info) {
    return reinterpret_cast<void*(__fastcall*)(void*, void*, void*, void*, const void*, const void*, void*)>(entry)(
        self, name, resource, asset, position, rotation, info);
}
inline void ForwardTimelineObject(void* entry, void* self, void* object, void* info) {
    reinterpret_cast<void(__fastcall*)(void*, void*, void*)>(entry)(self, object, info);
}
inline void ForwardTimelineDispose(void* entry, void* self, void* info) {
    reinterpret_cast<void(__fastcall*)(void*, void*)>(entry)(self, info);
}
inline void* ForwardTimelineResource(void* entry, void* self, void* name, void* info) {
    return reinterpret_cast<void*(__fastcall*)(void*, void*, void*)>(entry)(self, name, info);
}
inline bool ForwardTimelinePlaying(void* entry, void* self, void* name, void* info) {
    return reinterpret_cast<bool(__fastcall*)(void*, void*, void*)>(entry)(self, name, info);
}
inline bool ForwardTimelineRecent(void* entry, void* self, void* name, std::int32_t frames, void* info) {
    return reinterpret_cast<bool(__fastcall*)(void*, void*, std::int32_t, void*)>(entry)(self, name, frames, info);
}
inline void ForwardTimelineBehaviour(void* entry, void* object, const void* playable, const void* frame, void* info) {
    reinterpret_cast<void(__fastcall*)(void*, const void*, const void*, void*)>(entry)(object, playable, frame, info);
}
inline void ForwardTimelineProcess(void* entry, void* object, const void* playable, const void* frame, void* data,
                                   void* info) {
    reinterpret_cast<void(__fastcall*)(void*, const void*, const void*, void*, void*)>(entry)(object, playable, frame,
                                                                                              data, info);
}
inline void ForwardTimelineGraph(void* entry, void* object, const void* playable, void* info) {
    reinterpret_cast<void(__fastcall*)(void*, const void*, void*)>(entry)(object, playable, info);
}
}
