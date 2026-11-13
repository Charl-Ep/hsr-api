#pragma once
#include "../../src/presentation/events/AnimationConstructionRoute.hpp"
namespace hksr::api::detail {
template <auto Body> void __fastcall AnimationObjectEntry(void* object, void* invocation) {
    Body(object, invocation);
}
template <auto Body> void __fastcall AnimationTickEntry(void* object, float delta, void* invocation) {
    Body(object, delta, invocation);
}
template <auto Body>
void* __fastcall AnimationBuildEntry(void* context, void* configurations, void* parent, void* invocation) {
    return Body(context, configurations, parent, invocation);
}
template <auto Body>
void __fastcall AnimationConstructionEntry(void* receiver, void* events, void* behaviors, void* groups, void* logic,
                                           void* context, void* registration, void* file, void* invocation) {
    Body(AnimationConstructionArguments{receiver, events, behaviors, groups, logic, context, registration, file,
                                        invocation});
}
}
