#include <hksr/managed/ManagedRoots.hpp>
#include <map>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct ManagedRoots::State {
    References calls;
    bool weak=false;
    ManagedRootId sequence = 0;
    std::map<ManagedRootId, ManagedRootStatus> roots;
    explicit State(ReferenceMethods methods,bool observe) : calls(methods),weak(observe) {
        if (!methods.retain.code || !methods.get.code || !methods.free.code)
            throw std::invalid_argument("managed roots require retain, target and free methods");
    }
};
ManagedRoots::ManagedRoots(ReferenceMethods methods) : ManagedRoots(methods,false) {}
ManagedRoots::ManagedRoots(ReferenceMethods methods,bool weak) : state_(new State(methods,weak)) {}
ManagedRoots::~ManagedRoots() { delete state_; }
ManagedRootId ManagedRoots::Keep(void* object) {
    if (!object) throw std::invalid_argument("cannot retain a null managed input");
    const auto id = ++state_->sequence;
    auto& root = state_->roots.emplace(id, ManagedRootStatus{}).first->second;
    root.id = id; // allocate the receipt before the side effect
    root.retain = state_->weak?state_->calls.Watch(object):state_->calls.Retain(object);
    if (root.retain.call.returned && root.retain.handle) {
        root.target = state_->calls.Target(root.retain.handle);
        root.ready = root.target.call.returned && root.target.object == object;
    }
    if (!root.ready) Release(id);
    return id;
}
ObjectResult ManagedRoots::Target(ManagedRootId id) {
    auto& root = state_->roots.at(id);
    if (!root.ready || root.release_attempted) return {};
    root.target = state_->calls.Target(root.retain.handle);
    return root.target; // a read failure doesn't discard a still-owned handle
}
ManagedRootStatus ManagedRoots::Status(ManagedRootId id) const { return state_->roots.at(id); }
ManagedRootStatus ManagedRoots::Release(ManagedRootId id) {
    auto& root = state_->roots.at(id);
    root.ready = false;
    if (root.complete || root.release_attempted) return root;
    if (!root.retain.call.returned) return root; // unknown retain outcome
    if (!root.retain.handle) { root.complete = true; return root; }
    root.release_attempted = true; // never retry an unknown decrement
    root.free = state_->calls.Free(root.retain.handle);
    root.complete = root.free.returned;
    return root;
}
void ManagedRoots::Forget(ManagedRootId id) {
    if (!state_->roots.at(id).complete) throw std::logic_error("managed reference is not closed");
    state_->roots.erase(id);
}
}
