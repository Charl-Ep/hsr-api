#pragma once
#include "../entity/EntityAccess.hpp"
#include "../managed/ManagedRoots.hpp"
#include "AnimationControl.hpp"
#include "../unity/ComponentConfig.hpp"

namespace hksr::sdk_v1 {
enum class AnimationComponent { Model, FreeStyle, Emotion };
// Per-entity animation input ownership, not playback or a preview policy.
// All operations run on the owner's official thread/frame. Component results
// are borrowed current components: callers must recheck model/intent after a
// managed call. Types and owner stay rooted until Close; Text stays rooted until
// Drain. Never drain while a caller still uses a returned text. Destruction does
// not call the engine. Close is terminal and defers release during reentry.
class HKSR_API AnimationResources {
public:
    AnimationResources(EntityToken owner,ReferenceMethods references,EntityAccess entities);
    ~AnimationResources();
    AnimationResources(const AnimationResources&)=delete;
    AnimationResources& operator=(const AnimationResources&)=delete;
    void RetainOwner();
    ObjectResult Component(AnimationComponent kind,ManagedObjects& objects);
    // No type lookup/allocation; absent cache returns an empty result.
    ObjectResult CurrentComponent(AnimationComponent kind);
    void* Text(const std::string& value,ManagedObjects& objects);
    // Resolves exactly one current state identity and checks the game's own
    // round trip. Result is a value; temporary managed roots drain normally.
    TextResult StateName(void* animator,int hash,const AnimationControl&,const ComponentConfig&);
    void Drain();
    bool Close();
private:
    struct State;
    State* state_;
};
}
