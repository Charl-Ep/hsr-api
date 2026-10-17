#pragma once
#include <hksr/Export.hpp>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace hksr::sdk_v1::detail {
// Bookkeeping used by the SDK's shared presentation registry boundaries.
// This neither drives Directors nor owns managed references. The adapter roots objects,
// obtains lifetime tokens from actual entity creation, and reports every borrow
// and completed pool return. A caller's RuntimeID is deliberately not a key.
struct TimelineOwner {
    std::uintptr_t world = 0, entity = 0;
    std::uint64_t lifetime = 0;
    bool operator==(const TimelineOwner& b) const {
        return std::tie(world,entity,lifetime)==std::tie(b.world,b.entity,b.lifetime);
    }
};
struct TimelineRecordKey {
    std::uintptr_t registry = 0, record = 0;
    bool operator<(const TimelineRecordKey& b) const {
        return std::tie(registry,record)<std::tie(b.registry,b.record);
    }
};
struct TimelineTicket {
    TimelineRecordKey key;
    std::int32_t playback = 0;
    std::uint64_t borrow = 0;
};
enum class TimelinePhase { Available, Preparing, Playing, Paused, Stopped };
struct TimelineRecordView {
    TimelineRecordKey key;
    std::int32_t playback = 0;
    TimelinePhase phase = TimelinePhase::Available;
    std::string resource;
};
enum class TimelineVisibility { Public, Owned, Hidden, StaleObservation };
enum class TimelineWatch { Current, Finished, StaleObservation };
struct TimelineSelection {
    std::optional<TimelineRecordKey> record;
    // An inconsistent tracked row is not silently reclassified as public.
    // Independent, readable rows still participate in selection.
    std::vector<TimelineRecordKey> stale;
};

// Every recognized task invocation enters a scope, INCLUDING official tasks.
// Otherwise an official task reentering inside an owned completion callback
// would accidentally inherit that callback's private lookup domain.
// Native detours must explicitly close this in an SEH finally, as with the
// existing event projection; C++ RAII alone is not an SEH-cleanup guarantee.
class TimelineCallScope {
public:
    HKSR_API explicit TimelineCallScope(std::optional<TimelineOwner> owner,bool established=true);
    HKSR_API ~TimelineCallScope();
    TimelineCallScope(const TimelineCallScope&)=delete;
    TimelineCallScope& operator=(const TimelineCallScope&)=delete;
    HKSR_API static std::optional<TimelineOwner> Current();
    HKSR_API static bool Established();
    HKSR_API static bool Active();
    HKSR_API static void ExhaustResourceQueries();
    HKSR_API static bool ResourceQueriesAllowed();
private:
    static thread_local TimelineCallScope* current_;
    TimelineCallScope* previous_;
    std::optional<TimelineOwner> owner_;
    bool established_;
    bool resource_queries_=true;
};

class TimelineOwnership {
public:
    bool HasPrivate(std::uintptr_t registry) const {
        for(const auto& row:records_)if(row.first.registry==registry && row.second.owner)return true;
        return false;
    }
    std::optional<TimelineTicket> Ticket(TimelineRecordKey key) const {
        const auto i=records_.find(key);return i==records_.end()?std::nullopt:std::optional<TimelineTicket>(i->second.ticket);
    }
    std::optional<TimelineOwner> Owner(TimelineRecordKey key) const {
        const auto i=records_.find(key);return i==records_.end()?std::nullopt:std::optional<TimelineOwner>(i->second.owner);
    }
    // null owner means an observed OFFICIAL borrow, not "lookup failed".
    // Unknown/failed origin reads must be reported by the adapter separately.
    // complete=false retains an attributed partial write privately without
    // publishing it as a successful playback/query result.
    TimelineTicket Borrow(TimelineRecordKey key,std::int32_t playback,
        std::optional<TimelineOwner> owner,bool complete=true) {
        if(!key.registry || !key.record)throw std::invalid_argument("Timeline borrow needs registry and record identities");
        if(owner && (!owner->world || !owner->entity || !owner->lifetime))
            throw std::invalid_argument("Timeline owner needs world, entity and lifetime");
        const TimelineTicket ticket{key,playback,++sequence_};
        // Public borrows also need a generation: otherwise an official waiter
        // holding this pooled record could follow its next PRIVATE playback.
        records_.insert_or_assign(key,Entry{ticket,owner,false,!complete});
        return ticket;
    }
    // Call after the official return succeeded AND Available was observed,
    // not at Stop/request/callback entry. A nested borrow must survive the old
    // return's finally block. A failed/abnormal return leaves ownership intact.
    bool Returned(TimelineTicket ticket) {
        auto* entry=Find(ticket);
        if(!entry)return false;
        entry->returned=true;
        return true;
    }
    bool Removed(TimelineTicket ticket) {
        if(!Find(ticket))return false;
        records_.erase(ticket.key);return true;
    }
    // World/entity disposal is NOT evidence that this global registry returned
    // its Directors. Keep these rows private through outstanding cleanup.
    // Only an actual registry disposal discards all its rows.
    void RegistryDisposed(std::uintptr_t registry) {
        for(auto it=records_.begin();it!=records_.end();)
            if(it->first.registry==registry)it=records_.erase(it);else ++it;
    }
    TimelineVisibility Visibility(const TimelineRecordView& row,
        std::optional<TimelineOwner> caller) const {
        const auto it=records_.find(row.key);
        if(it==records_.end())return caller?TimelineVisibility::Hidden:TimelineVisibility::Public;
        const auto& entry=it->second;
        if(entry.ticket.playback!=row.playback)return TimelineVisibility::StaleObservation;
        if(!entry.owner)return caller || entry.incomplete?TimelineVisibility::Hidden:TimelineVisibility::Public;
        if(!caller || !(entry.owner==*caller) || entry.returned || entry.incomplete || row.phase==TimelinePhase::Available)
            return TimelineVisibility::Hidden;
        return TimelineVisibility::Owned;
    }
    std::optional<TimelineTicket> Watch(const TimelineRecordView& row,std::optional<TimelineOwner> caller) const {
        const auto visible=Visibility(row,caller);
        if(visible!=TimelineVisibility::Owned && visible!=TimelineVisibility::Public)return {};
        return Ticket(row.key); // an unobserved pre-install row is not a ticket
    }
    TimelineWatch CheckWatch(TimelineTicket ticket,const TimelineRecordView& row,std::optional<TimelineOwner> caller) const {
        if(ticket.key.registry!=row.key.registry || ticket.key.record!=row.key.record)
            return TimelineWatch::StaleObservation;
        const auto* entry=Find(ticket);
        // Includes reuse by the SAME owner; a task must not inherit a later
        // playback just because a pooled EBMM/Director address is unchanged.
        if(!entry || !(entry->owner==caller) || entry->returned || entry->incomplete)return TimelineWatch::Finished;
        if(entry->ticket.playback!=row.playback)return TimelineWatch::StaleObservation;
        if(row.phase==TimelinePhase::Available || row.phase==TimelinePhase::Stopped)return TimelineWatch::Finished;
        return TimelineWatch::Current;
    }
    // Equivalent choice order to the audited ordinary IODNDPEJOFF branch,
    // AFTER caller visibility: first Playing, otherwise last Paused/Stopped.
    // The adapter supplies actual official list order and normalized resource.
    // Empty text is a valid string here; a null managed string is not modeled.
    TimelineSelection Select(const std::vector<TimelineRecordView>& rows,
        const std::string& resource,std::optional<TimelineOwner> caller) const {
        TimelineSelection result;
        for(const auto& row:rows) {
            if(row.resource!=resource)continue;
            const auto visibility=Visibility(row,caller);
            if(visibility==TimelineVisibility::StaleObservation){result.stale.push_back(row.key);continue;}
            if(visibility==TimelineVisibility::Hidden)continue;
            if(row.phase==TimelinePhase::Playing){result.record=row.key;break;}
            if(row.phase==TimelinePhase::Paused || row.phase==TimelinePhase::Stopped)result.record=row.key;
        }
        return result;
    }
    // A failed owned lookup does not authorize public stop-through. The adapter
    // filters actual records, keeps official name/phase selection, then calls
    // the selected record's original stop. Name text and resource stay intact.
    std::size_t Size() const {return records_.size();}
    bool HasPending(TimelineOwner owner) const {
        for(const auto& row:records_)
            if(row.second.owner==owner && !row.second.returned)return true;
        return false;
    }
    // Stopped is deliberately NOT retired: the registry still owes its return.
    bool Pending(TimelineTicket ticket) const {
        const auto* entry=Find(ticket);return entry && !entry->returned;
    }
private:
    struct Entry {TimelineTicket ticket;std::optional<TimelineOwner> owner;bool returned,incomplete;};
    std::map<TimelineRecordKey,Entry> records_;
    std::uint64_t sequence_=0;
    Entry* Find(TimelineTicket ticket) {
        const auto it=records_.find(ticket.key);
        return it!=records_.end() && it->second.ticket.borrow==ticket.borrow &&
            it->second.ticket.playback==ticket.playback?&it->second:nullptr;
    }
    const Entry* Find(TimelineTicket ticket) const {
        const auto it=records_.find(ticket.key);
        return it!=records_.end() && it->second.ticket.borrow==ticket.borrow &&
            it->second.ticket.playback==ticket.playback?&it->second:nullptr;
    }
};
}
