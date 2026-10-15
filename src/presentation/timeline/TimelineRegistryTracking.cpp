#include "TimelineRegistryTracking.hpp"
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/unity/ComponentConfig.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <sstream>
#include <set>
#include <hksr/managed/ManagedLists.hpp>

namespace hksr::sdk_v1::detail {
struct TimelineRegistryTracking::State {
    MethodResolver& methods;ObjectMetadata& metadata;ManagedObjects& objects;
    ReferenceMethods references;TimelineRegistryAccess::Fields fields;Trace trace;
    ManagedRoots roots;ComponentConfig strings;TimelineOwnership ownership;
    Method clone,history_set;void* history_field=nullptr;
    std::size_t history_offset=0;
    Method query_set;void* query_field=nullptr;std::size_t query_offset=0;
    using HistoryKey=std::tuple<void*,std::uintptr_t,std::uintptr_t,std::uint64_t>;
    struct History {void* list=nullptr;ManagedRootId root=0,registry_root=0;};
    std::map<HistoryKey,History> histories;
    std::vector<ManagedRootId> reflection_roots;
    static constexpr const char* HistoryType="System.Collections.Generic.List<System.Collections.Generic.KeyValuePair<System.String,System.Int32>>";
    struct Pins {ManagedRootId registry=0,record=0;};
    std::map<TimelineRecordKey,Pins> pins;
    std::vector<ManagedRootId> unresolved;
    struct Invocation {
        TimelineRegistryAccess access;
        std::optional<TimelineOwner> source;
        std::optional<TimelineTicket> ticket;
        std::unique_ptr<TimelineCallScope> callback_scope;
        ManagedRootId name_root=0;
        void* private_name=nullptr;
        bool established=false;
        Invocation(State& s):access(s.methods,s.metadata,s.references,s.fields){}
    };
    State(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,ReferenceMethods r,TimelineRegistryAccess::Fields f,Trace log)
        :methods(m),metadata(d),objects(o),references(r),trace(std::move(log)),roots(r),strings(ResolveComponentConfigMethods(m)) {
        // Declarations are domain-scoped: resolve once, not again for each
        // playback/callback. Runtime object values are still read at the boundary.
        const auto registry=f("","HPPNAAGGAMP"),record=f("","EBMMGODFEEO");
        fields=[registry,record](const char*,const char* type) {
            if(std::string(type)=="HPPNAAGGAMP")return registry;
            if(std::string(type)=="EBMMGODFEEO")return record;
            throw std::logic_error("unexpected Timeline schema request");
        };
        clone=m.Resolve("System","Object","MemberwiseClone","System.Object",{});
        unsigned found=0;
        for(const auto& field:registry)if(field.owner_namespace.empty() && field.owner_name=="HPPNAAGGAMP" && field.name=="DFNAOFNEBHC") {
            if(field.type!=HistoryType || !field.flags_observed || (field.flags&0x10))throw std::runtime_error("Timeline history field declaration differs");
            history_offset=field.offset;++found;
        }
        if(found!=1)throw std::runtime_error("Timeline history field absent or ambiguous");
        found=0;
        for(const auto& field:registry)if(field.owner_namespace.empty() && field.owner_name=="HPPNAAGGAMP" && field.name=="MKDJOLKJJON") {
            if(field.type!="System.Collections.Generic.List<EBMMGODFEEO>" || !field.flags_observed || (field.flags&0x10))
                throw std::runtime_error("Timeline query list declaration differs");
            query_offset=field.offset;++found;
        }
        if(found!=1)throw std::runtime_error("Timeline query list absent or ambiguous");
    }
    void Log(const std::string& text) noexcept {try{if(trace)trace("Timeline ownership: "+text);}catch(...) {}}
    void Event(const char* operation,TimelineTicket ticket,std::optional<TimelineOwner> owner) {
        if(!trace)return;
        std::ostringstream text;text<<operation<<" registry="<<reinterpret_cast<void*>(ticket.key.registry)
            <<" record="<<reinterpret_cast<void*>(ticket.key.record)<<" playback="<<ticket.playback<<" borrow="<<ticket.borrow;
        if(owner)text<<" world="<<reinterpret_cast<void*>(owner->world)<<" entity="<<reinterpret_cast<void*>(owner->entity)<<" lifetime="<<owner->lifetime;
        else text<<" source=official";
        Log(text.str());
    }
    ManagedRootId Keep(void* object) {
        const auto id=roots.Keep(object);
        if(!roots.Status(id).ready){unresolved.push_back(id);throw std::runtime_error("playback GC retention failed");}
        return id;
    }
    void Release(ManagedRootId id) {
        if(!id)return;
        if(roots.Release(id).complete)roots.Forget(id);
        else {unresolved.push_back(id);Log("GC release unresolved; receipt retained");}
    }
    void Pin(TimelineRecordKey key) {
        auto& p=pins[key]; // receipt before either engine call
        if(!p.registry)p.registry=Keep(reinterpret_cast<void*>(key.registry));
        if(!p.record)p.record=Keep(reinterpret_cast<void*>(key.record));
    }
    void Unpin(TimelineRecordKey key) {
        const auto i=pins.find(key);if(i==pins.end())return;
        Release(i->second.record);Release(i->second.registry);pins.erase(i);
    }
    void* HistoryList(void* registry) {
        void* list=nullptr;
        if(!native::CopyMemoryChecked(&list,static_cast<unsigned char*>(registry)+history_offset,sizeof(list)) || !list)
            throw std::runtime_error("Timeline history list unavailable");
        return list;
    }
    void* Reflect(ObjectResult value) {
        if(!value.call.returned || !value.object)throw std::runtime_error("Timeline history field reflection failed");
        reflection_roots.reserve(reflection_roots.size()+1);reflection_roots.push_back(Keep(value.object));return value.object;
    }
    void ResolveHistoryField() {
        if(history_field)return;
        auto* type=Reflect(objects.ManagedType("","HPPNAAGGAMP"));
        auto* name=Reflect(objects.NewStringUtf8("DFNAOFNEBHC"));
        const auto getter=methods.Resolve("System","RuntimeType","GetField","System.Reflection.FieldInfo",{"System.String","System.Reflection.BindingFlags"});
        ObjectResult result;result.call=native::Call(getter,&result.object,type,name,54);
        auto* field=Reflect(result);const auto actual=metadata.ReadObjectType(field);MetadataResolver resolver({actual});
        history_set=resolver.Resolve(actual.name_space.c_str(),actual.name.c_str(),"SetValue","System.Void",
            {"System.Object","System.Object","System.Reflection.BindingFlags","System.Reflection.Binder","System.Globalization.CultureInfo"});
        history_field=field;
    }
    void HistoryView(Invocation& inv,TimelineBoundaryCall& call,TimelineOwner owner) {
        const HistoryKey key{call.registry,owner.world,owner.entity,owner.lifetime};
        auto& entry=histories[key];
        if(!entry.registry_root)entry.registry_root=Keep(call.registry);
        if(!entry.list) {
            auto* source=HistoryList(call.registry);inv.access.Hold(source);
            const auto actual=metadata.ReadObjectType(source);MetadataResolver resolver({actual});
            // A new, empty list via the ACTUAL constructed generic method.
            // No HPP constructor (it resets the global playback counter), no
            // cloned List backing array, and no guessed KeyValuePair ABI.
            const auto range=resolver.Resolve(actual.name_space.c_str(),actual.name.c_str(),"GetRange",HistoryType,{"System.Int32","System.Int32"});
            void* list=nullptr;
            if(!native::Call(range,&list,source,0,0).returned || !list || list==source)
                throw std::runtime_error("Timeline private history allocation failed");
            entry.root=Keep(list);entry.list=list;
        }
        void* view=nullptr;
        if(!native::Call(clone,&view,call.registry).returned || !view || view==call.registry)
            throw std::runtime_error("Timeline registry view allocation failed");
        inv.access.Hold(view);ResolveHistoryField();
        if(!native::CallVoid(history_set,history_field,view,entry.list,0,static_cast<void*>(nullptr),static_cast<void*>(nullptr)).returned ||
            HistoryList(view)!=entry.list)
            throw std::runtime_error("Timeline registry view history assignment failed");
        // Only the call receiver changes. The global registry, real records,
        // Dictionary, pool, binding objects and callbacks are NOT replaced.
        // Original Return appends before callbacks, including abnormal returns.
        call.receiver=view;
    }
    void ForgetHistories(void* registry=nullptr) {
        for(auto i=histories.begin();i!=histories.end();) {
            if(registry && std::get<0>(i->first)!=registry){++i;continue;}
            Release(i->second.root);Release(i->second.registry_root);i=histories.erase(i);
        }
    }
    void QueryView(Invocation& inv,TimelineBoundaryCall& call) {
        // Unknown callers do not inherit an outer actor. They use the public
        // view, not a fabricated empty query. With no private records, the
        // official function runs directly, without inspecting its list.
        if(!inv.source && !ownership.HasPrivate(reinterpret_cast<std::uintptr_t>(call.registry)))return;
        const auto rows=inv.access.Rows(call.registry);
        for(const auto& error:rows.errors)Log("query view: "+error);
        if(!rows.complete)throw std::runtime_error("query list incomplete; no partial result published");
        std::vector<int> hidden;
        for(std::size_t i=0;i<rows.records.size();++i) {
            const auto& row=rows.records[i];const auto visible=ownership.Visibility(row.view,inv.source);
            if(visible==TimelineVisibility::StaleObservation) {
                // A stale PUBLIC observation cannot remove an official row.
                if(!inv.source && !ownership.Owner(row.view.key))continue;
                throw std::runtime_error("query view playback attribution stale");
            }
            if(visible==TimelineVisibility::Hidden)hidden.push_back(static_cast<int>(i));
        }
        if(hidden.empty())return;
        void* list=nullptr;
        if(!native::CopyMemoryChecked(&list,static_cast<unsigned char*>(call.registry)+query_offset,sizeof(list)) || !list)
            throw std::runtime_error("query list unavailable");
        inv.access.Hold(list);const auto actual=metadata.ReadObjectType(list);MetadataResolver resolver({actual});
        ManagedReferenceList access(ResolveReferenceListMethods(actual,"EBMMGODFEEO"));
        const auto count=access.Count(list);
        if(!count.read.returned || count.value!=static_cast<int>(rows.records.size()))throw std::runtime_error("query list changed");
        const auto range=resolver.Resolve(actual.name_space.c_str(),actual.name.c_str(),"GetRange",
            "System.Collections.Generic.List<EBMMGODFEEO>",{"System.Int32","System.Int32"});
        void* filtered=nullptr;
        if(!native::Call(range,&filtered,list,0,count.value).returned || !filtered || filtered==list)
            throw std::runtime_error("query list copy failed");
        inv.access.Hold(filtered);
        const auto remove=resolver.Resolve(actual.name_space.c_str(),actual.name.c_str(),"RemoveAt","System.Void",{"System.Int32"});
        for(auto i=hidden.rbegin();i!=hidden.rend();++i)
            if(!native::CallVoid(remove,filtered,*i).returned)throw std::runtime_error("query list filtering failed");
        if(!query_field) {
            auto* type=Reflect(objects.ManagedType("","HPPNAAGGAMP"));auto* name=Reflect(objects.NewStringUtf8("MKDJOLKJJON"));
            const auto getter=methods.Resolve("System","RuntimeType","GetField","System.Reflection.FieldInfo",{"System.String","System.Reflection.BindingFlags"});
            ObjectResult result;result.call=native::Call(getter,&result.object,type,name,54);
            auto* field=Reflect(result);const auto field_type=metadata.ReadObjectType(field);MetadataResolver field_methods({field_type});
            query_set=field_methods.Resolve(field_type.name_space.c_str(),field_type.name.c_str(),"SetValue","System.Void",
                {"System.Object","System.Object","System.Reflection.BindingFlags","System.Reflection.Binder","System.Globalization.CultureInfo"});
            query_field=field;
        }
        void* view=nullptr;
        if(!native::Call(clone,&view,call.registry).returned || !view || view==call.registry)throw std::runtime_error("query receiver copy failed");
        inv.access.Hold(view);
        if(!native::CallVoid(query_set,query_field,view,filtered,0,static_cast<void*>(nullptr),static_cast<void*>(nullptr)).returned)
            throw std::runtime_error("query receiver assignment failed");
        void* adopted=nullptr;
        if(!native::CopyMemoryChecked(&adopted,static_cast<unsigned char*>(view)+query_offset,sizeof(adopted)) || adopted!=filtered)
            throw std::runtime_error("query receiver did not adopt private list");
        call.receiver=view; // publish only a fully built view; never alter shared registry
    }
    void Commit(const TimelineRegistryRecord& row,const std::optional<TimelineOwner>& source,bool complete=true) {
        Pin(row.view.key);
        const auto ticket=ownership.Borrow(row.view.key,row.view.playback,source,complete);
        Event(complete?"borrow observed":"incomplete borrow retained privately",ticket,source);
    }
    void Cleanup(Invocation& inv) noexcept {
        try {Release(inv.name_root);inv.name_root=0;if(!inv.access.Close())Log("boundary read GC cleanup incomplete");}
        catch(...){Log("boundary cleanup exception; receipts retained");}
    }
    void* Begin(TimelineBoundaryCall& call) {
        const bool query=call.operation==TimelineBoundary::StopName || call.operation==TimelineBoundary::AssetPlaying ||
            call.operation==TimelineBoundary::ResourceLookup;
        if(!TimelineCallScope::Current() &&
            (call.operation==TimelineBoundary::RecentCompletion ||
             (query && TimelineCallScope::ResourceQueriesAllowed() && !ownership.HasPrivate(reinterpret_cast<std::uintptr_t>(call.registry)))))
            return nullptr; // public fast path: no registry inspection or replacement
        auto inv=std::make_unique<Invocation>(*this);
        try {
            inv->access.Hold(call.registry);inv->access.Hold(call.argument);
            inv->access.Hold(call.resource);inv->access.Hold(call.asset);
            inv->established=TimelineCallScope::Established();inv->source=TimelineCallScope::Current();
            if(call.operation==TimelineBoundary::Borrow) {
                if(inv->established && inv->source) {
                    if(!call.argument)throw std::runtime_error("owned runtime name is null; resource not used as substitute");
                    const auto text=strings.ReadText(call.argument);
                    if(!text.read.returned)throw std::runtime_error("owned runtime name unreadable");
                    // Preserve original text for official history substring
                    // semantics. A fresh reference identifies a partial Borrow;
                    // ownership selection, not a name prefix, isolates Stop.
                    const auto named=objects.NewStringUtf8(text.utf8.c_str());
                    if(!named.call.returned || !named.object)throw std::runtime_error("private runtime name allocation failed");
                    inv->name_root=Keep(named.object);inv->private_name=named.object;
                    call.argument=named.object;
                }
            }else if(call.operation==TimelineBoundary::RecentCompletion) {
                // Unknown must not inherit private history, but it is not a
                // reason to fabricate a negative OFFICIAL completion result.
                if(inv->source)HistoryView(*inv,call,*inv->source);
            }else if(call.operation==TimelineBoundary::StopName || call.operation==TimelineBoundary::AssetPlaying ||
                     call.operation==TimelineBoundary::ResourceLookup) {
                if(call.operation==TimelineBoundary::ResourceLookup && !TimelineCallScope::ResourceQueriesAllowed()) {
                    call.handled=true;call.result=nullptr; // an explicitly retired private binding only
                }else {
                    try {QueryView(*inv,call);}
                    catch(const std::exception& e) {
                        Log(std::string("query isolation incomplete: ")+e.what()+
                            (inv->source?"; owned query withheld":"; official query forwarded"));
                        // Never publish a partial list. Known private callers
                        // cannot fall back onto official playbacks. For all
                        // other callers the original retains its own result
                        // and exception; observation failure is not 'absent'.
                        call.receiver=nullptr;
                        if(inv->source){call.handled=true;call.result=nullptr;call.boolean_result=false;call.stop_record=nullptr;}
                    }
                }
            }else if(call.operation==TimelineBoundary::Return || call.operation==TimelineBoundary::Remove) {
                auto* record=call.operation==TimelineBoundary::Return?inv->access.Find(call.registry,call.argument):call.argument;
                if(record) {
                    const auto row=inv->access.Read(call.registry,record);
                    inv->ticket=ownership.Ticket(row.view.key);
                    inv->source=ownership.Owner(row.view.key);
                    // Stale ID is an observation failure, not permission to
                    // apply a newer return to the last known borrow.
                    if(inv->ticket && inv->ticket->playback!=row.view.playback) {
                        Log("return/removal playback ID disagrees; old ticket not consumed");inv->ticket.reset();
                    }
                }
                // Completion callbacks inherit the record, NOT whichever actor
                // happened to cause this official update/return call.
                // An untracked return is an UNKNOWN barrier, not proof of an
                // official origin, and must not inherit an outer owned scope.
                inv->callback_scope=std::make_unique<TimelineCallScope>(inv->source,inv->ticket.has_value());
                if(call.operation==TimelineBoundary::Return && inv->ticket && inv->source)HistoryView(*inv,call,*inv->source);
            }
            return inv.release();
        }catch(...){Cleanup(*inv);throw;}
    }
    void End(void* receipt,const TimelineBoundaryCall& call,void* result,bool abnormal) noexcept {
        std::unique_ptr<Invocation> inv(static_cast<Invocation*>(receipt));if(!inv)return;
        try {
            if(call.operation==TimelineBoundary::Borrow) {
                if(!inv->established) {
                    // Missing origin is not a positively identified official
                    // borrow; never erase a private marker on this assumption.
                    Log("borrow has no established task/callback origin; attribution unresolved");
                }else if(!abnormal && result) {
                    auto* record=inv->access.Find(call.registry,result);
                    if(!record)throw std::runtime_error("returned Director absent from registry dictionary");
                    Commit(inv->access.Read(call.registry,record),inv->source);
                }else if(inv->private_name) {
                    // The ordinary Borrow stores this exact, newly allocated
                    // reference before Transform operations and before new ID.
                    // Match pointer identity, not coordinates, name text, list
                    // position or an assumed increment. Never fabricate success.
                    const auto rows=inv->access.Rows(call.registry);std::size_t matches=0;
                    for(const auto& row:rows.records)if(row.runtime_name==inv->private_name) {
                        Commit(row,inv->source,false);++matches;
                    }
                    Log("borrow incomplete; exact-name-reference records="+std::to_string(matches)+
                        " list_complete="+std::to_string(rows.complete));
                    for(const auto& error:rows.errors)Log(error);
                }else if(abnormal)Log("official borrow aborted; existing private markers retained");
            }else if(!abnormal) {
                if(call.operation==TimelineBoundary::Return && inv->ticket) {
                    auto* record=inv->access.Find(call.registry,call.argument);
                    if(!record)throw std::runtime_error("returned record missing; return is not removal");
                    const auto row=inv->access.Read(call.registry,record);
                    if(row.view.phase==TimelinePhase::Available && row.view.playback==inv->ticket->playback) {
                        if(ownership.Returned(*inv->ticket))Event("returned Available",*inv->ticket,inv->source);
                        else Event("old return receipt ignored",*inv->ticket,inv->source);
                    }
                    else Log("normal pool return did not leave captured playback Available; receipt retained");
                }
                if(call.operation==TimelineBoundary::Remove && inv->ticket && ownership.Removed(*inv->ticket)) {
                    Event("record removed",*inv->ticket,inv->source);Unpin(inv->ticket->key);
                }
                if(call.operation==TimelineBoundary::Dispose) {
                    const auto registry=reinterpret_cast<std::uintptr_t>(call.registry);
                    ownership.RegistryDisposed(registry);
                    for(auto i=pins.begin();i!=pins.end();) {
                        const auto key=i++->first;if(key.registry==registry)Unpin(key);
                    }
                    ForgetHistories(call.registry);
                }
            }else Log("official registry operation aborted; ownership retained");
        }catch(const std::exception& e){Log(e.what());}
        catch(...){Log("boundary reconciliation exception; ownership not certified");}
        Cleanup(*inv);
    }
};
TimelineRegistryTracking::TimelineRegistryTracking(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,
    ReferenceMethods refs,TimelineRegistryAccess::Fields fields,Trace trace)
    :state_(std::make_unique<State>(m,d,o,refs,std::move(fields),trace)) {
    // Check the complete declared record schema before installing any boundary.
    // This creates no game object and reads no live registry.
    TimelineRegistryAccess schema(m,d,refs,state_->fields);
    hooks_=std::make_unique<TimelineRegistryHooks>(m,[this](auto& call){return state_->Begin(call);},
        [this](auto* receipt,const auto& call,auto* result,bool abnormal){state_->End(receipt,call,result,abnormal);},std::move(trace));
}
TimelineRegistryTracking::~TimelineRegistryTracking() {
    hooks_->Stop();
    if(!state_->pins.empty() || !state_->histories.empty() || !state_->reflection_roots.empty() || !state_->unresolved.empty())state_.release(); // no engine calls here
}
void TimelineRegistryTracking::ActivateOnCurrentThread(){hooks_->ActivateOnCurrentThread();}
const TimelineOwnership& TimelineRegistryTracking::Ownership() const{return state_->ownership;}
std::optional<TimelineRecordView> TimelineRegistryTracking::ObserveRecord(void* record) {
    std::optional<TimelineRecordKey> key;
    for(const auto& item:state_->pins)if(item.first.record==reinterpret_cast<std::uintptr_t>(record)) {
        if(key)throw std::runtime_error("Timeline record belongs to more than one retained registry");
        key=item.first;
    }
    if(!key)return {};
    TimelineRegistryAccess access(state_->methods,state_->metadata,state_->references,state_->fields);
    try {
        const auto row=access.Read(reinterpret_cast<void*>(key->registry),record);
        if(!access.Close())state_->Log("wait read GC cleanup incomplete");
        return row.view;
    }catch(...){access.Close();throw;}
}
bool TimelineRegistryTracking::Close() {
    hooks_->Stop();
    while(!state_->pins.empty())state_->Unpin(state_->pins.begin()->first);
    state_->ForgetHistories();
    for(const auto id:state_->reflection_roots)state_->Release(id);
    state_->reflection_roots.clear();
    return state_->unresolved.empty();
}
std::optional<TimelinePlaybackSource> TimelineRegistryTracking::ObserveDirector(void* director) {
    if(!director)return {};
    std::set<std::uintptr_t> registries;
    for(const auto& p:state_->pins)registries.insert(p.first.registry);
    TimelineRegistryAccess access(state_->methods,state_->metadata,state_->references,state_->fields);
    try {
        access.Hold(director);std::optional<TimelinePlaybackSource> result;
        for(const auto registry:registries) {
            auto* manager=reinterpret_cast<void*>(registry);
            auto* record=access.Find(manager,director);
            if(!record)continue;
            const auto row=access.Read(manager,record);
            const auto owner=state_->ownership.Owner(row.view.key);
            const auto ticket=state_->ownership.Watch(row.view,owner);
            if(!ticket || state_->ownership.CheckWatch(*ticket,row.view,owner)!=TimelineWatch::Current)continue;
            if(result)throw std::runtime_error("Director has multiple current registry borrows");
            result=TimelinePlaybackSource{*ticket,owner};
        }
        if(!access.Close())state_->Log("graph source GC cleanup incomplete");
        return result;
    }catch(...){access.Close();throw;}
}
std::size_t TimelineRegistryTracking::CollectHistories(const std::function<bool(TimelineOwner)>& alive) {
    if(!alive)throw std::invalid_argument("history retirement requires source liveness");
    if(TimelineCallScope::Active())throw std::logic_error("cannot retire history inside a Timeline callback");
    std::size_t removed=0;
    for(auto i=state_->histories.begin();i!=state_->histories.end();) {
        const TimelineOwner owner{std::get<1>(i->first),std::get<2>(i->first),std::get<3>(i->first)};
        if(state_->ownership.HasPending(owner) || alive(owner)){++i;continue;}
        state_->Release(i->second.root);state_->Release(i->second.registry_root);
        i=state_->histories.erase(i);++removed;
    }
    return removed;
}
}
