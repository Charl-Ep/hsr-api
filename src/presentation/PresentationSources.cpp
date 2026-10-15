#include <hksr/presentation/PresentationSources.hpp>
#include <map>
#include <stdexcept>
#include <exception>

namespace hksr::sdk_v1 {
namespace {
bool Same(EntityOrigin a,EntityOrigin b) {
    return a.world==b.world && a.token.entity==b.token.entity && a.token.generation==b.token.generation;
}
}
struct PresentationSources::State {
    EntityProvenance entities;
    ManagedWeakReferences weak;
    struct Request {EffectReference reference;EntityOrigin owner;ManagedRootId identity=0;};
    std::map<void*,Request> requests;
    std::vector<ManagedRootId> unresolved;
    bool closed=false,coverage=true;
    explicit State(ReferenceMethods r):entities(r),weak(r){}
    void Release(ManagedRootId id) {
        if(!id)return;
        if(weak.Release(id).complete)weak.Forget(id);else unresolved.push_back(id);
    }
    bool Alive(const Request& r) {
        if(!r.identity)throw std::runtime_error("explicit effect source retention unresolved");
        const auto t=weak.Target(r.identity);
        if(!t.call.returned)throw std::runtime_error("explicit effect source weak read failed");
        if(t.object && t.object!=r.reference.parameters)throw std::runtime_error("explicit effect source identity changed");
        return t.object!=nullptr;
    }
    void Open() const {if(closed)throw std::logic_error("presentation sources closed");}
};
PresentationSources::PresentationSources(ReferenceMethods r):state_(std::make_unique<State>(r)){}
PresentationSources::~PresentationSources() {
    if(!state_->requests.empty() || !state_->unresolved.empty())state_.release(); // no engine calls
}
void PresentationSources::Remember(EntityOrigin origin) {
    state_->Open();
    // Ordinary handle failure leaves that entity's own unresolved receipt in
    // EntityProvenance. It must NOT invalidate every unrelated official source.
    // Only native allocation failure may have prevented recording the identity.
    try{state_->entities.Remember(origin);}catch(const std::bad_alloc&){state_->coverage=false;throw;}
}
EntityOrigin PresentationSources::Identify(GameEntity* entity) {
    state_->Open();
    if(!entity)throw std::runtime_error("presentation source entity absent");
    const auto origin=state_->entities.Find(entity);
    if(!origin.token.entity && !state_->coverage)throw std::runtime_error("presentation birth coverage incomplete");
    return origin;
}
void PresentationSources::RememberRequest(EffectReference ref,GameEntity* owner) {
    auto& s=*state_;s.Open();
    if(!ref.parameters)throw std::invalid_argument("explicit effect request absent");
    const auto origin=Identify(owner);
    if(!origin.token.entity)throw std::invalid_argument("explicit effect owner has no Framework birth receipt");
    auto i=s.requests.find(ref.parameters);
    if(i!=s.requests.end()) {
        if(s.Alive(i->second) && i->second.reference.id==ref.id) {
            if(!Same(i->second.owner,origin))throw std::logic_error("same effect request has conflicting owners");
            return;
        }
        s.Release(i->second.identity);s.requests.erase(i);
    }
    auto& r=s.requests.emplace(ref.parameters,State::Request{ref,origin}).first->second;
    r.identity=s.weak.Watch(ref.parameters);
    if(!s.weak.Status(r.identity).ready)throw std::runtime_error("explicit effect source weak allocation failed");
}
void PresentationSources::RetireRequest(EffectReference ref) {
    auto& s=*state_;s.Open();const auto i=s.requests.find(ref.parameters);
    if(i==s.requests.end() || i->second.reference.id!=ref.id)return;
    s.Release(i->second.identity);s.requests.erase(i);
}
EntityOrigin PresentationSources::Identify(EffectReference ref,const EffectRelationsSnapshot& facts) {
    auto& s=*state_;s.Open();
    if(!facts.same_generation || !facts.id_before.read.returned || !facts.id_after.read.returned ||
        facts.id_before.value!=ref.id || facts.id_after.value!=ref.id)
        throw std::runtime_error("effect source request generation unresolved");
    const auto i=s.requests.find(ref.parameters);
    if(i!=s.requests.end()) {
        if(s.Alive(i->second) && i->second.reference.id==ref.id)return i->second.owner;
        s.Release(i->second.identity);s.requests.erase(i);
    }
    if(!facts.caster.call.returned || !facts.caster.object)
        throw std::runtime_error("effect caster absent/unreadable and no explicit receipt; target is not source");
    return Identify(static_cast<GameEntity*>(facts.caster.object));
}
bool PresentationSources::HasSource(EntityOrigin origin) {
    auto& s=*state_;s.Open();
    // Contains reads the stored weak receipt; it never dereferences this old address.
    if(s.entities.Contains(origin))return true;
    for(const auto& r:s.requests)if(Same(r.second.owner,origin) && s.Alive(r.second))return true;
    return false;
}
void PresentationSources::Collect() {
    auto& s=*state_;s.Open();std::exception_ptr failure;
    try{s.entities.Collect();}catch(...){failure=std::current_exception();}
    for(auto i=s.requests.begin();i!=s.requests.end();) {
        try{if(s.Alive(i->second)){++i;continue;}}
        catch(...){if(!failure)failure=std::current_exception();++i;continue;}
        s.Release(i->second.identity);i=s.requests.erase(i);
    }
    if(failure)std::rethrow_exception(failure);
}
bool PresentationSources::Close() {
    auto& s=*state_;s.closed=true;
    for(const auto& r:s.requests)s.Release(r.second.identity);
    s.requests.clear();const auto complete=s.entities.Close();
    return complete && s.unresolved.empty();
}
}
