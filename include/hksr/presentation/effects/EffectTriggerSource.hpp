#pragma once
#include "EffectSubmissions.hpp"
#include <hksr/unity/ComponentConfig.hpp>

namespace hksr::sdk_v1 {
// Reads the original configured path inside an attributed submission callback.
// The path labels a source node; it is not permission to search/delete effects.
class EffectTriggerSource {
public:
    HKSR_API EffectTriggerSource(const std::vector<DeclaredField>&,ReferenceMethods,ComponentConfig);
    HKSR_API TextResult Path(const EffectTriggerExecution&) const;
private:
    std::size_t path_=0;
    ReferenceMethods references_;
    ComponentConfig strings_;
};
}
