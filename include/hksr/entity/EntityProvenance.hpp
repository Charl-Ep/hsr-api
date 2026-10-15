#pragma once
#include "EntityLifecycle.hpp"
#include "../managed/ManagedRoots.hpp"

namespace hksr::sdk_v1 {
// Engine-thread registry of ACTUAL creation receipts, separate from live
// ownership. Populate during creation, not on the first later Timeline call.
// Dispose/Forget in EntityLifecycle do not change this historical identity.
// Weak references avoid retaining the entity/world or confusing reused memory.
class HKSR_API EntityProvenance {
public:
    explicit EntityProvenance(ReferenceMethods);
    ~EntityProvenance();
    EntityProvenance(const EntityProvenance&)=delete;
    EntityProvenance& operator=(const EntityProvenance&)=delete;
    void Remember(EntityOrigin);
    // Caller holds this managed entity alive. Empty means no tracked origin;
    // the consumer must also know its creation observation coverage is complete.
    EntityOrigin Find(GameEntity*);
    // Tests a stored receipt through its weak handle, without dereferencing an
    // old entity address or requiring the caller to hold that entity alive.
    bool Contains(EntityOrigin);
    void Collect(); // clear observations whose managed objects were collected
    bool Close();  // explicit engine-thread handle cleanup, no game disposal
private:
    struct State;State* state_;
};
}
