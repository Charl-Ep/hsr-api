#include <hksr/entity/EntityProvenance.hpp>
#include <map>
#include <stdexcept>
#include <exception>

namespace hksr::sdk_v1 {
struct EntityProvenance::State {
    struct Entry {EntityOrigin origin;ManagedRootId weak=0;};
    ManagedWeakReferences refs;
    std::map<GameEntity*,Entry> entries;
    std::vector<ManagedRootId> unresolved;
    bool closed=false;
    explicit State(ReferenceMethods methods):refs(methods){}
    void Release(ManagedRootId id) {
        if(!id)return;
        if(refs.Release(id).complete)refs.Forget(id);else unresolved.push_back(id);
    }
    void* Target(const Entry& entry) {
        if(!entry.weak)throw std::runtime_error("entity provenance allocation unresolved");
        const auto result=refs.Target(entry.weak);
        if(!result.call.returned)throw std::runtime_error("entity provenance weak target unavailable");
        if(result.object && result.object!=entry.origin.token.entity)
            throw std::runtime_error("entity provenance target identity changed");
        return result.object;
    }
};
EntityProvenance::EntityProvenance(ReferenceMethods refs):state_(new State(refs)){}
EntityProvenance::~EntityProvenance() {
    // No engine operations from an arbitrary destructor thread. Preserve
    // unresolved receipts if the owning host violates explicit Close.
    if(state_->entries.empty() && state_->unresolved.empty())delete state_;
}
void EntityProvenance::Remember(EntityOrigin origin) {
    auto& s=*state_;
    if(s.closed || !origin.token.entity || !origin.token.generation || !origin.world)
        throw std::invalid_argument("entity provenance needs an open registry and full birth receipt");
    auto existing=s.entries.find(origin.token.entity);
    if(existing!=s.entries.end()) {
        const auto live=s.Target(existing->second);
        if(live && existing->second.origin.token.generation==origin.token.generation) {
            if(existing->second.origin.world!=origin.world)throw std::logic_error("same birth changed source world");
            return;
        }
        s.Release(existing->second.weak);s.entries.erase(existing);
    }
    // Reserve the native slot before creating a managed handle.
    auto slot=s.entries.emplace(origin.token.entity,State::Entry{origin}).first;
    slot->second.weak=s.refs.Watch(origin.token.entity);
    // Retain a failed identity receipt; never turn a failed observation public.
    if(!s.refs.Status(slot->second.weak).ready)throw std::runtime_error("entity provenance weak allocation failed");
}
EntityOrigin EntityProvenance::Find(GameEntity* entity) {
    auto& s=*state_;
    if(s.closed)throw std::logic_error("entity provenance is closed");
    const auto entry=s.entries.find(entity);if(entry==s.entries.end())return {};
    if(s.Target(entry->second))return entry->second.origin;
    s.Release(entry->second.weak);s.entries.erase(entry);return {};
}
void EntityProvenance::Collect() {
    auto& s=*state_;
    std::exception_ptr failure;
    for(auto i=s.entries.begin();i!=s.entries.end();) {
        try{if(s.Target(i->second)){++i;continue;}}
        catch(...){if(!failure)failure=std::current_exception();++i;continue;}
        s.Release(i->second.weak);i=s.entries.erase(i);
    }
    if(failure)std::rethrow_exception(failure);
}
bool EntityProvenance::Contains(EntityOrigin origin) {
    auto& s=*state_;
    if(s.closed)throw std::logic_error("entity provenance is closed");
    const auto i=s.entries.find(origin.token.entity);
    if(i==s.entries.end() || i->second.origin.world!=origin.world ||
        i->second.origin.token.generation!=origin.token.generation)return false;
    return s.Target(i->second)!=nullptr;
}
bool EntityProvenance::Close() {
    auto& s=*state_;s.closed=true;
    for(const auto& entry:s.entries)s.Release(entry.second.weak);
    s.entries.clear();return s.unresolved.empty();
}
}
