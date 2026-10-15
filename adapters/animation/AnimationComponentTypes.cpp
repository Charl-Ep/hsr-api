#include <hksr/animation/AnimationResources.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
ObjectResult ResolveAnimationComponentType(ManagedObjects& objects,AnimationComponent kind) {
    switch(kind) {
    case AnimationComponent::Model:return objects.ManagedType("RPG.GameCore","CharacterModelComponent");
    case AnimationComponent::FreeStyle:return objects.ManagedType("RPG.GameCore","FreeStyleComponent");
    case AnimationComponent::Emotion:return objects.ManagedType("","PEPNFLNCDHG");
    }
    throw std::invalid_argument("unknown animation component");
}
}
