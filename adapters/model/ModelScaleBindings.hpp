#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <functional>
namespace hksr::api::detail {
struct ModelScaleBindings {
    RuntimeType multiplier_type, config_type, helper_type;
    Method helper_ctor, apply, multiplier, dict_ctor, dict_add, contains, item, shader_get, alive, multiplier_set;
    void* multiplier_field = nullptr;
    void* factor_key = nullptr;
    std::size_t factors = 0, outline = 0, config_outline = 0, config_equal = 0;
};
using ModelScaleRoot = std::function<void*(ObjectResult)>;
ModelScaleBindings ResolveModelScaleBindings(RuntimeMetadata&, const ModelScaleRoot&);
}
