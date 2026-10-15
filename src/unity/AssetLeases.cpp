#include <hksr/unity/AssetLeases.hpp>
#include <map>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct AssetLeases::State {
    ReferenceMethods methods;
    References calls;
    AssetLeaseId sequence = 0;
    std::map<AssetLeaseId, AssetLeaseStatus> leases;
    explicit State(ReferenceMethods input) : methods(input), calls(input) {}
};
AssetLeases::AssetLeases(ReferenceMethods methods) : state_(new State(methods)) {}
AssetLeases::~AssetLeases() { delete state_; }
AssetLeaseId AssetLeases::Acquire(void* path, void* type, bool option) {
    // These are the actual acquire/cleanup dependencies; unrelated inspection
    // APIs are not prerequisites. Check before performing an acquisition.
    const auto& m=state_->methods;
    if (!m.load_asset.code || !m.release_asset.code || !m.retain.code || !m.get.code || !m.free.code)
        throw std::runtime_error("asset acquisition requires its release and GC reference methods");
    const auto id=++state_->sequence;
    // Allocate the receipt before the engine side effect, not after it returns.
    auto& lease=state_->leases.emplace(id,AssetLeaseStatus{}).first->second;
    lease.id=id;
    lease.load=state_->calls.LoadAsset(path,type,option);
    if (!lease.load.call.returned) return id; // unknown acquisition; no invented object to release
    if (!lease.load.object) {lease.complete=true;return id;}
    lease.root=state_->calls.Retain(lease.load.object);
    if (lease.root.call.returned && lease.root.handle) {
        lease.target=state_->calls.Target(lease.root.handle);
        lease.ready=lease.target.call.returned && lease.target.object==lease.load.object;
    }
    // The receipt already owns any successful load even when retaining fails.
    // Release that acquisition immediately on the same engine thread.
    if (!lease.ready) Release(id);
    return id;
}
void* AssetLeases::Object(AssetLeaseId id) const {
    const auto& lease=state_->leases.at(id);
    return lease.ready ? lease.load.object : nullptr;
}
AssetLeaseStatus AssetLeases::Status(AssetLeaseId id) const { return state_->leases.at(id); }
AssetLeaseStatus AssetLeases::Release(AssetLeaseId id) {
    auto& lease=state_->leases.at(id);
    lease.ready=false;
    if (lease.complete || !lease.load.call.returned || !lease.load.object) return lease;
    if (lease.asset_release==ReleaseState::NotAttempted) {
        lease.asset_release=ReleaseState::OutcomeUnknown;
        lease.release=state_->calls.ReleaseAsset(lease.load.object);
        if (lease.release.returned) lease.asset_release=ReleaseState::Returned;
    }
    if (lease.asset_release!=ReleaseState::Returned) return lease;
    if (lease.root.handle && lease.root_release==ReleaseState::NotAttempted) {
        lease.root_release=ReleaseState::OutcomeUnknown;
        lease.free=state_->calls.Free(lease.root.handle);
        if (lease.free.returned) lease.root_release=ReleaseState::Returned;
    }
    // A failing Retain could have created an unreported handle. Do not turn
    // "resource released" into "all references released" in that case.
    lease.complete=lease.root.call.returned && (!lease.root.handle || lease.root_release==ReleaseState::Returned);
    return lease;
}
void AssetLeases::Forget(AssetLeaseId id) {
    if (!state_->leases.at(id).complete) throw std::logic_error("lease still carries unresolved cleanup responsibility");
    state_->leases.erase(id);
}
}
