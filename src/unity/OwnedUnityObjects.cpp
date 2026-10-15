#include <hksr/unity/OwnedUnityObjects.hpp>
#include <hksr/native/Invocation.hpp>
#include <map>
#include <stdexcept>

namespace hksr::sdk_v1 {
Observed<bool> UnityObjectAlive(Method alive,void* object) {
    Observed<bool> out;
    if(alive.code && object) out.read=native::Call(alive,&out.value,object);
    return out;
}
struct OwnedUnityObjects::State {
    struct Entry { void* original = nullptr; ManagedRootId root = 0; UnityObjectStatus status; };
    UnityLifetimeMethods methods;
    ManagedRoots roots;
    UnityObjectId sequence = 0;
    std::map<UnityObjectId, Entry> entries;
    State(UnityLifetimeMethods calls, ReferenceMethods references) : methods(calls), roots(references) {
        if (!calls.alive.code || !calls.destroy.code) throw std::invalid_argument("owned Unity lifetime needs alive and Destroy");
    }
};
OwnedUnityObjects::OwnedUnityObjects(UnityLifetimeMethods methods, ReferenceMethods references)
    : state_(new State(methods, references)) {}
OwnedUnityObjects::~OwnedUnityObjects() { delete state_; }
UnityObjectId OwnedUnityObjects::Adopt(void* object) {
    if (!object) throw std::invalid_argument("cannot adopt a null Unity object");
    for (const auto& pair : state_->entries)
        if (pair.second.original == object && !pair.second.status.complete)
            throw std::logic_error("Unity object already owned in this collection");
    const auto id = ++state_->sequence;
    auto& entry = state_->entries.emplace(id, State::Entry{}).first->second;
    entry.original = object; entry.status.id = id;
    entry.root = state_->roots.Keep(object);
    entry.status.root = state_->roots.Status(entry.root);
    return id;
}
UnityObjectStatus OwnedUnityObjects::Status(UnityObjectId id) const { return state_->entries.at(id).status; }
UnityObjectStatus OwnedUnityObjects::Close(UnityObjectId id) {
    return Drain(id, true);
}
UnityObjectStatus OwnedUnityObjects::ObserveEnd(UnityObjectId id) {
    return Drain(id, false);
}
UnityObjectStatus OwnedUnityObjects::Drain(UnityObjectId id, bool request_destroy) {
    auto& entry = state_->entries.at(id);
    auto& status = entry.status;
    if (status.complete) return status;
    if (status.native_destroyed) {
        status.root = state_->roots.Release(entry.root);
        status.complete = status.root.complete;
        return status;
    }
    // Failed retention does not grant permission to dereference an unrooted
    // stale address next frame. Keep the unresolved native responsibility.
    if (!status.root.ready) return status;
    status.target = state_->roots.Target(entry.root);
    if (!status.target.call.returned || !status.target.object) return status;
    status.alive = UnityObjectAlive(state_->methods.alive,status.target.object);
    if (!status.alive.read.returned) return status;
    if (!status.alive.value) {
        status.native_destroyed = true;
        status.root = state_->roots.Release(entry.root);
        status.complete = status.root.complete;
    } else if (request_destroy && !status.destroy_attempted) {
        status.destroy_attempted = true; // fault may happen after scheduling
        status.destroy = native::CallVoid(state_->methods.destroy, status.target.object);
    }
    return status;
}
std::vector<UnityObjectStatus> OwnedUnityObjects::CloseAll() {
    std::vector<UnityObjectStatus> result;
    result.reserve(state_->entries.size());
    for (const auto& pair : state_->entries) result.push_back(Close(pair.first));
    return result;
}
void OwnedUnityObjects::Forget(UnityObjectId id) {
    if (!state_->entries.at(id).status.complete) throw std::logic_error("native object ownership not closed");
    state_->roots.Forget(state_->entries.at(id).root);
    state_->entries.erase(id);
}
}
