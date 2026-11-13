#pragma once
#include "../../src/presentation/effects/EffectLayoutNative.hpp"
#include <functional>
namespace hksr::api::detail {
EffectLayoutNativeBindings ResolveEffectLayoutBindings(RuntimeMetadata&,
                                                       const std::function<void*(ObjectResult)>& retain);
}
