#pragma once
#include "../managed/ManagedRoots.hpp"
#include <vector>

namespace hksr::sdk_v1 {
struct UnityLifetimeMethods { Method alive, destroy; };
// Read-only; caller retains the managed reference. Does not acquire ownership
// or schedule destruction, so it can also observe a borrowed renderer.
HKSR_API Observed<bool> UnityObjectAlive(Method alive,void* rooted_object);
using UnityObjectId = std::uint64_t;
struct UnityObjectStatus {
    UnityObjectId id = 0;
    ObjectResult target;
    Observed<bool> alive;
    CallResult destroy;
    ManagedRootStatus root;
    bool destroy_attempted = false, native_destroyed = false, complete = false;
};
// Adopt only newly created/independently cloned Unity objects whose native
// lifetime belongs to the caller. NEVER adopt a loaded shared template,
// pooled official root or a player's mesh. Asset leases are a separate API.
// Engine thread only; explicit Close polls official liveness after Destroy.
// Keep this owner alive until closed or engine shutdown. Destructor calls no game.
class HKSR_API OwnedUnityObjects {
public:
    OwnedUnityObjects(UnityLifetimeMethods methods, ReferenceMethods references);
    ~OwnedUnityObjects();
    OwnedUnityObjects(const OwnedUnityObjects&) = delete;
    OwnedUnityObjects& operator=(const OwnedUnityObjects&) = delete;
    UnityObjectId Adopt(void* owned_object);
    UnityObjectStatus Status(UnityObjectId id) const;
    UnityObjectStatus Close(UnityObjectId id);
    // Observe destruction owned by an official lifecycle or a private parent.
    // Never schedules Destroy. Frees this owner's GC root only once native-dead.
    UnityObjectStatus ObserveEnd(UnityObjectId id);
    std::vector<UnityObjectStatus> CloseAll();
    void Forget(UnityObjectId id);
private:
    UnityObjectStatus Drain(UnityObjectId id, bool request_destroy);
    struct State;
    State* state_;
};
HKSR_API UnityLifetimeMethods ResolveUnityLifetimeMethods(MethodResolver& source);
}
