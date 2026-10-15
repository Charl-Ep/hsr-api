#pragma once
#include "../managed/References.hpp"

namespace hksr::sdk_v1 {
using AssetLeaseId = std::uint64_t;
enum class ReleaseState { NotAttempted, Returned, OutcomeUnknown };
struct AssetLeaseStatus {
    AssetLeaseId id = 0;
    ObjectResult load, target;
    RootResult root;
    CallResult release, free;
    ReleaseState asset_release = ReleaseState::NotAttempted;
    ReleaseState root_release = ReleaseState::NotAttempted;
    bool ready = false;
    bool complete = false;
};
// Explicit engine-thread operations, not a destructor that may call the engine
// on an arbitrary thread. Keep this service alive until its leases are closed
// or the engine has ended. Failed side effects remain visible, never retried
// just because the next frame requests cleanup again.
class HKSR_API AssetLeases {
public:
    explicit AssetLeases(ReferenceMethods methods);
    ~AssetLeases();
    AssetLeases(const AssetLeases&) = delete;
    AssetLeases& operator=(const AssetLeases&) = delete;
    AssetLeaseId Acquire(void* managed_path, void* managed_type, bool option);
    void* Object(AssetLeaseId id) const;
    AssetLeaseStatus Status(AssetLeaseId id) const;
    AssetLeaseStatus Release(AssetLeaseId id);
    // Discard only a completed receipt. This never releases an engine object.
    void Forget(AssetLeaseId id);
private:
    struct State;
    State* state_;
};
}
