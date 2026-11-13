#pragma once
#include <cstdint>
namespace hksr::api::detail {
template <auto Body>
void* __fastcall TimelineBorrowEntry(void* self, void* name, void* resource, void* asset, const void* position,
                                     const void* rotation, void* invocation) {
    return Body(self, name, resource, asset, position, rotation, invocation);
}
template <auto Body> void __fastcall TimelineObjectEntry(void* self, void* object, void* invocation) {
    Body(self, object, invocation);
}
template <auto Body> void __fastcall TimelineDisposeEntry(void* self, void* invocation) {
    Body(self, invocation);
}
template <auto Body> void* __fastcall TimelineResourceEntry(void* self, void* name, void* invocation) {
    return Body(self, name, invocation);
}
template <auto Body> bool __fastcall TimelinePlayingEntry(void* self, void* name, void* invocation) {
    return Body(self, name, invocation);
}
template <auto Body>
bool __fastcall TimelineRecentEntry(void* self, void* name, std::int32_t frames, void* invocation) {
    return Body(self, name, frames, invocation);
}
template <auto Body>
void __fastcall TimelineBehaviourEntry(void* object, const void* playable, const void* frame, void* invocation) {
    Body(object, playable, frame, invocation);
}
template <auto Body>
void __fastcall TimelineProcessEntry(void* object, const void* playable, const void* frame, void* data,
                                     void* invocation) {
    Body(object, playable, frame, data, invocation);
}
template <auto Body> void __fastcall TimelineGraphEntry(void* object, const void* playable, void* invocation) {
    Body(object, playable, invocation);
}
}
