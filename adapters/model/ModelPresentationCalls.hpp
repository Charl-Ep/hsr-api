#pragma once
#include <hksr/model/ActorModelPresentation.hpp>
#include <hksr/native/Invocation.hpp>
#include <cstddef>

namespace hksr::api::detail {
struct ModelScaleFollowInput {
    void* target = nullptr;
    void* point = nullptr;
    bool late = false, outline = false;
    unsigned char padding[6]{};
    void* multiplier = nullptr;
};
static_assert(sizeof(ModelScaleFollowInput) == 32 && offsetof(ModelScaleFollowInput, late) == 16 &&
              offsetof(ModelScaleFollowInput, multiplier) == 24);
inline CallResult WriteModelScaleFollow(Method method, void* component, void* rooted_target) {
    ModelScaleFollowInput input;
    input.target = rooted_target;
    return native::CallVoid(method, component, &input);
}
inline CallResult WriteFaceMaterial(Method method, void* component, FaceMaterialTransition input) {
    return native::CallVoid(method, component, input.value.cheek, input.value.shy, input.value.shadow, input.duration);
}
inline CallResult WriteModelScaleFactor(Method method, void* model, void* rooted_key, float value) {
    return native::CallVoid(method, model, rooted_key, value, true);
}
inline CallResult ConstructModelScaleHelper(Method method, void* helper, void* config) {
    return native::CallVoid(method, helper, static_cast<void*>(nullptr), config);
}
inline CallResult ReadModelScaleMultiplier(Method method, float* value, void* helper, GameEntity* target) {
    return native::Call(method, value, helper, target);
}
inline CallResult ApplyModelScaleHelper(Method method, void* helper, GameEntity* target, float base, float ratio) {
    return native::CallVoid(method, helper, target, base, ratio);
}
}
