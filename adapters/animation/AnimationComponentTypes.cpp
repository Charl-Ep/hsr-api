#include <hksr/animation/AnimationResources.hpp>
#include <stdexcept>

namespace hksr::api {
ObjectResult ResolveAnimationComponentType(ManagedObjects& objects, AnimationComponent kind) {
    switch (kind) {
    case AnimationComponent::Model:
        return objects.ManagedType("RPG.GameCore", "CharacterModelComponent");
    case AnimationComponent::FreeStyle:
        return objects.ManagedType("RPG.GameCore", "FreeStyleComponent");
    case AnimationComponent::Emotion:
        return objects.ManagedType("", "PEPNFLNCDHG");
    case AnimationComponent::Motion:
        return objects.ManagedType("", "BGOBCKLPEMK");
    case AnimationComponent::Transform:
        return objects.ManagedType("RPG.GameCore", "TransformComponent");
    }
    throw std::invalid_argument("unknown animation component");
}
}
