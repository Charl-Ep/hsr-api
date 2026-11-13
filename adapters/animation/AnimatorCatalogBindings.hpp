#pragma once
#include "AnimatorLayout.hpp"
#include <hksr/animation/AnimatorCatalog.hpp>

namespace hksr::api::detail {
struct AnimatorCatalogBindings {
    Method cached, layers, layer_name, resolve, hash, has, initialized;
    AnimatorLayout layout;
};
AnimatorCatalogBindings ResolveAnimatorCatalog(RuntimeMetadata&);
ControllerCatalog ReadAnimatorController(const AnimatorCatalogBindings&, void* native, int layers);
}
