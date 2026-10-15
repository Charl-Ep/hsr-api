#pragma once
#include "../entity/EntityProvenance.hpp"
#include "effects/EffectRelations.hpp"
#include <memory>

namespace hksr::sdk_v1 {
// Engine-thread sources, not live-entity permission. Explicit request receipts
// cover Framework requests whose official input has no CasterEntity.
class PresentationSources {
public:
    HKSR_API explicit PresentationSources(ReferenceMethods);
    HKSR_API ~PresentationSources();
    PresentationSources(const PresentationSources&)=delete;
    PresentationSources& operator=(const PresentationSources&)=delete;
    HKSR_API void Remember(EntityOrigin);
    HKSR_API void RememberRequest(EffectReference,GameEntity* owner);
    // Called only after official release/reuse was observed; exact old receipt.
    HKSR_API void RetireRequest(EffectReference);
    HKSR_API EntityOrigin Identify(GameEntity*);
    HKSR_API EntityOrigin Identify(EffectReference,const EffectRelationsSnapshot&);
    HKSR_API bool HasSource(EntityOrigin);
    HKSR_API void Collect();
    HKSR_API bool Close();
private:
    struct State;std::unique_ptr<State> state_;
};
}
