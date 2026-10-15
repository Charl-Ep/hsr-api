#pragma once
#include "../../animation/OfficialCatalog.hpp"

namespace hksr::sdk_v1 {
// Builds a private, one-state animation group using ORIGINAL RemoveEffect
// configurations addressed by official ability + field path. No task runs here.
// The caller registers Groups through AnimationEvents::RegisterInline, retaining
// it before Close. This is a preview-exit adaptation, not official skill timing.
class HKSR_API AnimationExitConfig {
public:
    AnimationExitConfig(MethodResolver&,ObjectMetadata&,ManagedObjects&,TypeRelationships&,ReferenceMethods,ArrayMethods);
    ~AnimationExitConfig();
    AnimationExitConfig(const AnimationExitConfig&)=delete;
    AnimationExitConfig& operator=(const AnimationExitConfig&)=delete;
    bool Prepare(const std::string& state,const std::vector<EffectRemovalSource>&);
    void* Groups() const;
    const std::vector<std::string>& Errors() const;
    bool Close(); // GC roots only; never deregisters or runs task cleanup
private:struct State;State* state_;
};
}
