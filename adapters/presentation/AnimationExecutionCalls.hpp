#pragma once
#include "../../src/presentation/events/AnimationConstructionRoute.hpp"
namespace hksr::api::detail {
inline void ForwardAnimationVoid(void* entry, void* object, void* info) {
    reinterpret_cast<void(__fastcall*)(void*, void*)>(entry)(object, info);
}
inline void ForwardAnimationTick(void* entry, void* object, float delta, void* info) {
    reinterpret_cast<void(__fastcall*)(void*, float, void*)>(entry)(object, delta, info);
}
inline void* ForwardAnimationTaskBuild(void* entry, void* context, void* configurations, void* parent, void* info) {
    return reinterpret_cast<void*(__fastcall*)(void*, void*, void*, void*)>(entry)(context, configurations, parent,
                                                                                   info);
}
inline void ForwardAnimationConstruction(void* entry, const AnimationConstructionArguments& args) {
    reinterpret_cast<void(__fastcall*)(void*, void*, void*, void*, void*, void*, void*, void*, void*)>(entry)(
        args.receiver, args.events, args.behaviors, args.groups, args.logic, args.context, args.registration, args.file,
        args.metadata);
}
}
