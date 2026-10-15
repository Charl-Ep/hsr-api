#include <hksr/presentation/events/EventRegistrationLease.hpp>
#include <algorithm>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct EventRegistrationLease::State {
public:
    State(ReferenceMethods references,AnimationEvents events,
        ManagedReferenceList lists):roots_(references),events_(events),lists_(lists){}
    void Prepare(const ManagedArrays& arrays,const SceneObjects& strings,
        ObjectResult baseline_paths,ObjectResult desired_paths);
    void PrepareInline(void* rooted_groups);
    void Attach(void* owned_entity,void* event_component,EntityToken owner_token={});
    void Service();
    EventRegistrationResult Retire();
    void OwnerEnded(); // notification only, never calls component/record methods
    // Caller observed a different component/consumer/context on the SAME live
    // entity. Not a claim that the old effects were successfully retired.
    void Superseded();
    EventRegistrationResult Close(); // GC only; does not silently retire a live consumer
    const EventInputStatus& Status() const { return status_; }
    const std::vector<EventFileReceipt>& Files() const { return files_; }
private:
    struct Snapshot { void* consumer=nullptr; void* context=nullptr; };
    void* Keep(void* object);
    Snapshot ReadIdentity();
    bool Current();
    std::vector<void*> ReadRecords(void* list);
    ManagedRoots roots_;
    std::vector<ManagedRootId> handles_;
    AnimationEvents events_;
    ManagedReferenceList lists_;
    void* entity_=nullptr;
    EntityToken owner_token_;
    void* component_=nullptr;
    Snapshot identity_;
    EventInputStatus status_;
    std::vector<EventFileReceipt> files_;
    bool preparation_attempted_=false;
};

namespace {
void Require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
bool Has(const std::vector<void*>& values,void* value) { return std::find(values.begin(),values.end(),value)!=values.end(); }
}
void* EventRegistrationLease::State::Keep(void* object) {
    Require(object!=nullptr,"event input object is null");
    handles_.push_back(0); const auto id=roots_.Keep(object); handles_.back()=id;
    Require(roots_.Status(id).ready,"event input root unavailable"); return object;
}
void EventRegistrationLease::State::Prepare(const ManagedArrays& arrays,const SceneObjects& strings,
    ObjectResult baseline_paths,ObjectResult desired_paths) {
    if(preparation_attempted_ || status_.closing) throw std::logic_error("event inputs are single-use");
    preparation_attempted_=true;
    try {
        auto read=[&](ObjectResult input) {
            Require(input.call.returned,"event path field unavailable");
            std::vector<void*> out; if(!input.object) return out;
            Keep(input.object); const auto count=arrays.Length(input.object);
            Require(count.read.returned && count.value>=0,"event path array unavailable");
            for(std::int32_t i=0;i<count.value;++i) {
                const auto item=arrays.Get(input.object,i);
                Require(item.call.returned && item.object,"event path array item unavailable");
                out.push_back(Keep(item.object));
            }
            return out;
        };
        const auto baseline=read(baseline_paths),desired=read(desired_paths);
        auto contains=[&](const std::vector<void*>& paths,void* path) {
            for(auto* other:paths) {
                if(other==path) return true;
                const auto same=strings.EqualNames(other,path);
                Require(same.read.returned,"event path comparison unavailable");
                if(same.value) return true;
            }
            return false;
        };
        std::vector<void*> selected;
        for(auto* path:desired) if(!contains(baseline,path) && !contains(selected,path)) selected.push_back(path);
        for(auto* path:selected) { EventFileReceipt file; file.path=path; files_.push_back(file); }
        status_.prepared=true;
    } catch(const std::exception& error) { status_.errors.push_back(error.what()); }
}
EventRegistrationLease::State::Snapshot EventRegistrationLease::State::ReadIdentity() {
    Snapshot out;
    const auto consumer=events_.Consumer(component_),context=events_.Context(component_);
    Require(consumer.call.returned && consumer.object,"event consumer unavailable");
    Require(context.call.returned && context.object,"event context unavailable");
    const auto caster=events_.Caster(context.object);
    Require(caster.call.returned && caster.object==entity_,"event caster is not the owned entity");
    out.consumer=consumer.object; out.context=context.object; return out;
}
void EventRegistrationLease::State::PrepareInline(void* groups) {
    if(preparation_attempted_ || status_.closing)throw std::logic_error("event inputs are single-use");
    preparation_attempted_=true;
    try {EventFileReceipt file;file.inline_groups=Keep(groups);files_.push_back(file);status_.prepared=true;}
    catch(const std::exception& e){status_.errors.push_back(e.what());}
}
void EventRegistrationLease::State::Attach(void* entity,void* component,EntityToken owner_token) {
    if(!status_.prepared || status_.attached || status_.closing || status_.owner_ended)
        throw std::logic_error("event input attachment phase invalid");
    try {
        Require(!owner_token.entity || owner_token.entity==entity,"event owner token belongs to another entity");
        owner_token_=owner_token;
        entity_=Keep(entity); component_=Keep(component);
        identity_=ReadIdentity(); Keep(identity_.consumer); Keep(identity_.context);
        status_.attached=true;
    } catch(const std::exception& error) { status_.errors.push_back(error.what()); }
}
bool EventRegistrationLease::State::Current() {
    if(!status_.attached || status_.owner_ended || status_.identity_changed) return false;
    if(owner_token_.entity && (TrackedEntities().IsDisposed(owner_token_) ||
        TrackedEntities().Current(owner_token_.entity).generation!=owner_token_.generation)) {
        OwnerEnded(); return false;
    }
    const auto now=ReadIdentity();
    // A list replacement alone is not a new consumer: registrations are queried
    // through the current official getter every time, never List's version field.
    if(now.consumer!=identity_.consumer || now.context!=identity_.context) {
        status_.identity_changed=true; status_.errors.push_back("official event consumer/context changed"); return false;
    }
    return true;
}
std::vector<void*> EventRegistrationLease::State::ReadRecords(void* list) {
    std::vector<void*> values;
    if(!list) return values;
    const auto count=lists_.Count(list);
    Require(count.read.returned && count.value>=0,"event record count unavailable");
    for(std::int32_t i=0;i<count.value;++i) {
        const auto value=lists_.Item(list,i);
        Require(value.call.returned,"event record read unavailable"); values.push_back(value.object);
    }
    return values;
}
void EventRegistrationLease::State::Service() {
    if(status_.closing || status_.owner_ended || !status_.attached) return;
    try {
        if(!Current()) return;
        std::vector<void*> records; bool observed=false;
        const bool has_prior=std::any_of(files_.begin(),files_.end(),[](const EventFileReceipt& file) { return file.register_attempted; });
        if(has_prior) try {
            const auto list=events_.Records(component_); Require(list.call.returned,"event registration list unavailable");
            records=ReadRecords(list.object); observed=true;
        } catch(const std::exception& error) { status_.errors={error.what()}; }
        for(auto& file:files_) {
            if(status_.owner_ended || status_.closing) break;
            if(file.register_attempted) {
                file.observed=observed;
                if(observed) file.present=file.registration.object && Has(records,file.registration.object);
                file.official_end=ObserveEventRecord(file.lifetime);
                // Only a known registration, actually absent AND with a returned
                // official Dispose, can be replenished in this same consumer.
                if(!observed || file.present || !file.official_end.returned) continue;
                file.previous.push_back(static_cast<const EventRegistrationReceipt&>(file));
                ForgetEventRecord(file.lifetime);
                // Completed old records need no long-lived GC retention. A
                // failed Free remains in the root ledger but cannot invalidate
                // the independent replacement registration.
                if(file.registration_root) roots_.Release(file.registration_root);
                static_cast<EventRegistrationReceipt&>(file)=EventRegistrationReceipt{};
            }
            if(!Current()) break;
            file.register_attempted=true;
            file.registration=file.inline_groups?events_.RegisterInline(component_,identity_.context,nullptr,nullptr,file.inline_groups):
                events_.Register(component_,identity_.context,file.path);
            try {
                Require(file.registration.call.returned && file.registration.object,"event registration outcome unresolved");
                Keep(file.registration.object); file.retained=true;
                file.registration_root=handles_.back();
                file.lifetime=TrackEventRecord(file.registration.object);
            } catch(const std::exception& error) { file.error=error.what(); }
        }
    } catch(const std::exception& error) { status_.errors={error.what()}; }
}
EventRegistrationResult EventRegistrationLease::State::Retire() {
    status_.closing=true; EventRegistrationResult out;
    if(status_.owner_ended || status_.retirement_complete) { out.complete=true; return out; }
    try {
        if(status_.attached && !Current()) throw std::runtime_error("event consumer no longer matches this incarnation");
        for(auto& file:files_) {
            if(status_.owner_ended) break;
            if(!file.register_attempted) continue;
            if(!file.registration.call.returned || !file.registration.object || !file.retained) {
                out.errors.push_back("event registration outcome unresolved"); continue;
            }
            try {
            if(!Current()) { out.errors.push_back("event consumer changed during retirement"); break; }
            file.official_end=ObserveEventRecord(file.lifetime);
            if(!file.detach_attempted) {
                const auto list=events_.Records(component_);
                if(!list.call.returned) { out.errors.push_back("event registration list unavailable"); continue; }
                const auto records=ReadRecords(list.object);
                file.observed=true; file.present=Has(records,file.registration.object);
                if(!file.present) {
                    if(!file.official_end.returned) out.errors.push_back("owned registration absent; official retirement not observed");
                    continue;
                }
                file.detach_attempted=true;
                file.detach=events_.Detach(component_,file.registration.object,true);
            }
            if(!file.detach.returned) { out.errors.push_back("event detach outcome unresolved"); continue; }
            if(status_.owner_ended) break;
            if(!Current()) { out.errors.push_back("event owner/consumer changed during detach"); break; }
            file.official_end=ObserveEventRecord(file.lifetime);
            if(file.official_end.returned) continue; // detach may dispatch official disposal itself
            if(file.official_end.entered && !file.dispose_attempted) {
                out.errors.push_back("official event dispose outcome unresolved"); continue;
            }
            if(!file.dispose_attempted) {
                file.dispose_attempted=true; file.dispose=events_.Dispose(file.registration.object);
            }
            if(!file.dispose.returned) out.errors.push_back("event dispose outcome unresolved");
            } catch(const std::exception& error) { out.errors.push_back(error.what()); }
        }
    } catch(const std::exception& error) { out.errors.push_back(error.what()); }
    status_.retirement_complete=out.errors.empty() && !status_.owner_ended;
    out.complete=out.errors.empty(); return out;
}
void EventRegistrationLease::State::OwnerEnded() { status_.owner_ended=true; status_.closing=true; }
void EventRegistrationLease::State::Superseded() { status_.identity_changed=true; status_.closing=true; }
EventRegistrationResult EventRegistrationLease::State::Close() {
    status_.closing=true; EventRegistrationResult out;
    if(status_.attached && !status_.owner_ended && !status_.retirement_complete && !status_.identity_changed) {
        out.errors.push_back("event registration retirement unresolved"); return out;
    }
    for(auto& file:files_) ForgetEventRecord(file.lifetime);
    for(auto id:handles_) if(id && !roots_.Release(id).complete) out.errors.push_back("event root release unresolved");
    status_.roots_closed=out.errors.empty(); out.complete=status_.roots_closed; return out;
}
EventRegistrationLease::EventRegistrationLease(ReferenceMethods references, AnimationEvents events, ManagedReferenceList lists)
    :state_(new State(references, events, lists)) {}
EventRegistrationLease::~EventRegistrationLease() { delete state_; }
void EventRegistrationLease::Prepare(const ManagedArrays& arrays, const SceneObjects& strings, ObjectResult baseline, ObjectResult desired) {
    state_->Prepare(arrays, strings, baseline, desired);
}
void EventRegistrationLease::PrepareInline(void* groups) { state_->PrepareInline(groups); }
void EventRegistrationLease::Attach(void* entity, void* component, EntityToken owner) { state_->Attach(entity, component, owner); }
void EventRegistrationLease::Service() { state_->Service(); }
EventRegistrationResult EventRegistrationLease::Retire() { return state_->Retire(); }
void EventRegistrationLease::OwnerEnded() { state_->OwnerEnded(); }
void EventRegistrationLease::Superseded() { state_->Superseded(); }
EventRegistrationResult EventRegistrationLease::Close() { return state_->Close(); }
const EventInputStatus& EventRegistrationLease::Status() const { return state_->Status(); }
const std::vector<EventFileReceipt>& EventRegistrationLease::Files() const { return state_->Files(); }
}
