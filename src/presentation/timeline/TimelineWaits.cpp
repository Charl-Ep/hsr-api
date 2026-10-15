#include "TimelineWaits.hpp"
#include <hksr/native/MetadataReader.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <array>

namespace hksr::sdk_v1::detail {
struct TimelineWaits::State {
    MethodResolver& methods;ObjectMetadata& metadata;ManagedObjects& objects;
    ManagedRoots roots;ManagedWeakReferences weak;TimelineRegistryTracking& registry;TimelineTasks::Trace trace;
    struct Field {std::size_t offset=0;void* field=nullptr;Method set;};
    std::array<Field,2> fields;
    struct Entry {
        ManagedRootId root=0;std::uint64_t epoch=0;
        std::optional<TimelineOwner> owner;
        std::optional<TimelineTicket> ticket;
        bool exhausted=false,source_known=false;
    };
    struct Receipt {void* task;std::uint64_t epoch;};
    std::map<void*,Entry> waits;
    std::vector<ManagedRootId> metadata_roots,unresolved,unresolved_weak;
    std::uint64_t sequence=0;
    bool closed=false;
    State(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,ReferenceMethods refs,
        TimelineRegistryAccess::Fields declarations,TimelineRegistryTracking& r,TimelineTasks::Trace log)
        :methods(m),metadata(d),objects(o),roots(refs),weak(refs),registry(r),trace(std::move(log)) {
        const std::array<const char*,2> types{{"JODBIGDDMOB","IGBEIECLHMC"}};
        for(std::size_t i=0;i<types.size();++i) {
            bool found=false;
            for(const auto& f:declarations("",types[i]))if(f.owner_namespace.empty() && f.owner_name==types[i] && f.name=="BMHGECJDFGN") {
                if(found || f.type!="EBMMGODFEEO" || !f.flags_observed || (f.flags&0x10))throw std::runtime_error("wait saved-record declaration differs");
                fields[i].offset=f.offset;found=true;
            }
            if(!found)throw std::runtime_error("wait saved-record declaration absent");
        }
    }
    void Log(const std::string& text) noexcept {try{if(trace)trace("Timeline wait: "+text);}catch(...) {}}
    ManagedRootId Keep(void* value) {
        const auto id=roots.Keep(value);
        if(!roots.Status(id).ready){unresolved.push_back(id);throw std::runtime_error("wait GC retention failed");}
        return id;
    }
    void Release(ManagedRootId id) {
        if(roots.Release(id).complete)roots.Forget(id);else {unresolved.push_back(id);Log("GC release unresolved; receipt retained");}
    }
    ManagedRootId Watch(void* object) {
        const auto id=weak.Watch(object);
        if(!weak.Status(id).ready){unresolved_weak.push_back(id);throw std::runtime_error("wait weak observation failed");}
        return id;
    }
    void ReleaseWeak(ManagedRootId id) {
        if(weak.Release(id).complete)weak.Forget(id);
        else {unresolved_weak.push_back(id);Log("weak handle release unresolved; receipt retained");}
    }
    bool Alive(void* object,const Entry& entry) {
        const auto result=weak.Target(entry.root);
        if(!result.call.returned)throw std::runtime_error("wait weak target unreadable");
        if(result.object && result.object!=object)throw std::runtime_error("wait managed identity changed");
        return result.object!=nullptr;
    }
    void* Metadata(ObjectResult result) {
        if(!result.call.returned || !result.object)throw std::runtime_error("wait field reflection failed");
        metadata_roots.reserve(metadata_roots.size()+1);metadata_roots.push_back(Keep(result.object));return result.object;
    }
    static std::size_t Index(TimelineTask t){return t==TimelineTask::Wait?0:1;}
    void* Read(const TimelineTaskCall& call) {
        void* value=nullptr;
        if(!native::CopyMemoryChecked(&value,static_cast<unsigned char*>(call.object)+fields[Index(call.task)].offset,sizeof(value)))
            throw std::runtime_error("wait saved-record unreadable");
        return value;
    }
    void Clear(const TimelineTaskCall& call) {
        auto& f=fields[Index(call.task)];
        if(!f.field) {
            const auto type=Metadata(objects.ManagedType("",call.task==TimelineTask::Wait?"JODBIGDDMOB":"IGBEIECLHMC"));
            auto* name=Metadata(objects.NewStringUtf8("BMHGECJDFGN"));
            const auto getter=methods.Resolve("System","RuntimeType","GetField","System.Reflection.FieldInfo",{"System.String","System.Reflection.BindingFlags"});
            ObjectResult value;value.call=native::Call(getter,&value.object,type,name,54);auto* field=Metadata(value);
            const auto actual=metadata.ReadObjectType(field);MetadataResolver resolver({actual});
            const auto setter=resolver.Resolve(actual.name_space.c_str(),actual.name.c_str(),"SetValue","System.Void",
                {"System.Object","System.Object","System.Reflection.BindingFlags","System.Reflection.Binder","System.Globalization.CultureInfo"});
            f.field=field;f.set=setter;
        }
        // Managed null assignment through FieldInfo; no raw GC reference write
        // and no fabricated stopped record or replacement Director.
        if(!native::CallVoid(f.set,f.field,call.object,static_cast<void*>(nullptr),0,static_cast<void*>(nullptr),static_cast<void*>(nullptr)).returned)
            throw std::runtime_error("wait saved-record clear failed");
        if(Read(call))throw std::runtime_error("wait saved-record clear not adopted");
    }
    void Forget(void* task) {
        const auto i=waits.find(task);if(i==waits.end())return;
        ReleaseWeak(i->second.root);waits.erase(i);
    }
    void Inspect(const TimelineTaskCall& call,Entry& entry) {
        auto* saved=Read(call);
        if(!entry.owner && !TimelineCallScope::Current()) {
            // Official-to-official pooling/retirement is the original task's
            // business. Intervene only on a positively identified PRIVATE
            // generation, not a missing/stale observation of an official one.
            if(saved && !entry.exhausted) {
                const auto row=registry.ObserveRecord(saved);
                if(row) {
                    const auto ticket=registry.Ownership().Ticket(row->key);
                    entry.exhausted=ticket && ticket->playback==row->playback &&
                        registry.Ownership().Owner(row->key).has_value();
                }
            }
            if(entry.exhausted) {
                if(saved)Clear(call);
                TimelineCallScope::ExhaustResourceQueries();
            }
            return;
        }
        if(!entry.source_known) {entry.owner=TimelineCallScope::Current();entry.source_known=true;}
        else if(!(entry.owner==TimelineCallScope::Current()))entry.exhausted=true;
        if(entry.exhausted) {
            if(saved)Clear(call);
            TimelineCallScope::ExhaustResourceQueries();return;
        }
        if(entry.ticket) {
            const auto current=registry.Ownership().Ticket(entry.ticket->key);
            if(!current || current->borrow!=entry.ticket->borrow || current->playback!=entry.ticket->playback ||
                !(entry.owner==TimelineCallScope::Current()) || reinterpret_cast<std::uintptr_t>(saved)!=entry.ticket->key.record) {
                entry.exhausted=true;
            }else {
                const auto row=registry.ObserveRecord(saved);
                if(!row)throw std::runtime_error("retained wait record lost its registry relation");
                const auto check=registry.Ownership().CheckWatch(*entry.ticket,*row,entry.owner);
                if(check==TimelineWatch::StaleObservation)throw std::runtime_error("wait record changed without borrow observation");
                // Stopped/Available themselves remain the original Tick's job.
                // Only retirement/reuse/source change requires detachment.
                if(check==TimelineWatch::Finished && row->phase!=TimelinePhase::Stopped && row->phase!=TimelinePhase::Available)
                    entry.exhausted=true;
            }
        }else if(saved) {
            const auto row=registry.ObserveRecord(saved);
            if(row) {
                const auto visible=registry.Ownership().Visibility(*row,entry.owner);
                if(visible==TimelineVisibility::StaleObservation)throw std::runtime_error("initial wait record attribution stale");
                if(visible==TimelineVisibility::Hidden)entry.exhausted=true;
                else entry.ticket=registry.Ownership().Watch(*row,entry.owner);
            }else if(entry.owner) {
                // A Framework task must have selected through the candidate
                // lookup. Absence is missing integration, not an official row.
                throw std::runtime_error("owned wait record bypassed observed registry query");
            }
            // Pre-install official wait with no observed row keeps original
            // behavior. A later private borrow is visible here and detaches it;
            // no assertion that its earlier unseen public generation is known.
        }
        if(entry.exhausted) {
            if(saved)Clear(call);
            TimelineCallScope::ExhaustResourceQueries();
            Log("retired binding detached; original task/camera update retained");
        }
    }
};
TimelineWaits::TimelineWaits(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,ReferenceMethods r,
    TimelineRegistryAccess::Fields f,TimelineRegistryTracking& registry,TimelineTasks::Trace trace)
    :state_(std::make_unique<State>(m,d,o,r,std::move(f),registry,std::move(trace))){}
TimelineWaits::~TimelineWaits() {
    if(!state_->waits.empty() || !state_->metadata_roots.empty() || !state_->unresolved.empty() || !state_->unresolved_weak.empty())state_.release();
}
void* TimelineWaits::Before(const TimelineTaskCall& call) {
    auto& s=*state_;
    if(s.closed || (call.task!=TimelineTask::Wait && call.task!=TimelineTask::WaitFinish))return nullptr;
    if(call.step==TimelineTaskStep::Begin)s.Forget(call.object);
    auto i=s.waits.find(call.object);
    if(i!=s.waits.end() && !s.Alive(call.object,i->second)) {s.Forget(call.object);i=s.waits.end();}
    if(i==s.waits.end()) {
        if(call.step!=TimelineTaskStep::Begin && call.step!=TimelineTaskStep::Tick)return nullptr;
        State::Entry e;e.epoch=++s.sequence;e.owner=TimelineCallScope::Current();e.source_known=TimelineCallScope::Established();
        i=s.waits.emplace(call.object,e).first;
        try{i->second.root=s.Watch(call.object);}catch(...){s.waits.erase(i);throw;}
    }
    // Unknown attribution does not manufacture a public owner.
    if(call.step==TimelineTaskStep::Tick && TimelineCallScope::Established())s.Inspect(call,i->second);
    return new State::Receipt{call.object,i->second.epoch};
}
void TimelineWaits::After(void* raw,const TimelineTaskCall& call,bool abnormal) {
    std::unique_ptr<State::Receipt> receipt(static_cast<State::Receipt*>(raw));if(!receipt)return;
    auto& s=*state_;const auto i=s.waits.find(call.object);
    if(i==s.waits.end() || i->second.epoch!=receipt->epoch || abnormal)return;
    if(call.step==TimelineTaskStep::Reset || call.step==TimelineTaskStep::Dispose){s.Forget(call.object);return;}
    if(TimelineCallScope::Established())s.Inspect(call,i->second);
}
bool TimelineWaits::Close() {
    auto& s=*state_;s.closed=true;
    while(!s.waits.empty())s.Forget(s.waits.begin()->first);
    for(auto id:s.metadata_roots)s.Release(id);s.metadata_roots.clear();
    return s.unresolved.empty() && s.unresolved_weak.empty();
}
void TimelineWaits::Collect() {
    auto& s=*state_;
    for(auto i=s.waits.begin();i!=s.waits.end();) {
        if(s.Alive(i->first,i->second)){++i;continue;}
        s.ReleaseWeak(i->second.root);i=s.waits.erase(i);
    }
}
}
