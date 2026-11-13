#pragma once
#include <hksr/presentation/effects/EffectLayouts.hpp>
#include <hksr/presentation/effects/EffectRelations.hpp>
namespace hksr::api::detail {
struct EffectLayoutNativeBindings {
    Method component, alive, perform[4], copy, clear, inherit_set;
    void* plugin_type = nullptr;
    void* inherit_field = nullptr;
    std::size_t trigger = 0, plugin_owner = 0, behavior = 0, behavior_owner = 0, enabled = 0, hidden = 0, fade = 0;
};
// Native-provider seam. Production preparation fills this exact binding once;
// the execution path is shared with synthetic request/GC/ABI tests.
HKSR_API EffectLayoutServices BindEffectLayoutNative(EffectLayoutNativeBindings, EffectAccess, EffectRelations,
                                                     ReferenceMethods);
std::shared_ptr<EffectLayouts> PrepareEffectLayouts(RuntimeMetadata&, EffectAccess, ReferenceMethods);
}
