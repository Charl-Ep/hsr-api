#include "../../../src/presentation/timeline/TimelineOwnership.hpp"
#include <iostream>
#include <thread>

namespace {
using namespace hksr::sdk_v1::detail;
void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
const TimelineOwner a{1,100,1},b{1,200,2},rebuilt{1,100,3},other_world{2,100,1};
TimelineRecordView Row(std::uintptr_t record,std::int32_t id,TimelinePhase phase,
    const char* path="shared.playable",std::uintptr_t registry=10) {
    return {{registry,record},id,phase,path};
}
bool Selected(const TimelineSelection& result,std::uintptr_t record) {
    return result.record && result.record->record==record;
}
}
void TestTimelineOwnership() {
    using P=TimelinePhase;using V=TimelineVisibility;using W=TimelineWatch;
    // Same resource, mixed order and multiple SDK owners: BOTH directions.
    TimelineOwnership ledger;
    auto official=Row(1,41,P::Playing),owned=Row(2,42,P::Playing),another=Row(3,43,P::Playing);
    const auto original=ledger.Borrow(owned.key,owned.playback,a);
    ledger.Borrow(another.key,another.playback,b);
    Check(Selected(ledger.Select({official,another,owned},"shared.playable",a),2),"owned query selected official/other actor");
    Check(Selected(ledger.Select({owned,another,official},"shared.playable",{}),1),"official query selected private playback");
    Check(!ledger.Select({official,another},"shared.playable",a).record,"missing owned playback fell back to global");
    Check(!ledger.Select({owned},"shared.playable",rebuilt).record,"rebuilt entity inherited old playback");
    Check(!ledger.Select({owned},"shared.playable",other_world).record,"world identity ignored");
    Check(!ledger.Select({owned},"different.playable",a).record,"resource identity ignored");

    // Choice order and terminal semantics come from the actual ordinary query,
    // not a made-up "latest ID wins" or "only active" policy.
    auto paused=Row(4,44,P::Paused),stopped=Row(5,45,P::Stopped),preparing=Row(6,46,P::Preparing);
    ledger.Borrow(paused.key,44,a);ledger.Borrow(stopped.key,45,a);ledger.Borrow(preparing.key,46,a);
    Check(Selected(ledger.Select({paused,stopped},"shared.playable",a),5),"last fallback not preserved");
    Check(Selected(ledger.Select({stopped,paused},"shared.playable",a),4),"fallback order changed");
    Check(Selected(ledger.Select({paused,owned,stopped},"shared.playable",a),2),"first Playing not preserved");
    Check(!ledger.Select({preparing},"shared.playable",a).record,"Preparing became a query result");
    Check(ledger.Visibility(preparing,a)==V::Owned,"Preparing lost cleanup ownership");
    auto public_paused=Row(7,47,P::Paused),public_stopped=Row(8,48,P::Stopped);
    Check(Selected(ledger.Select({public_paused,public_stopped},"shared.playable",{}),8),"official fallback order changed");

    // Completion callback runs before Available. Ownership remains during it;
    // a nested new borrow must not be erased by the outer return's completion.
    auto watch=ledger.Watch(owned,a);Check(watch.has_value(),"owned wait ticket absent");
    owned.phase=P::Stopped;
    Check(ledger.Visibility(owned,a)==V::Owned,"stopped callback lost source scope");
    Check(ledger.CheckWatch(*watch,owned,a)==W::Finished,"stopped watch remains executing");
    // The ordinary return has NOT made this record Available yet. A callback
    // can borrow ANOTHER record, not this still-Stopped one.
    auto callback_play=Row(9,49,P::Playing);
    const auto callback_ticket=ledger.Borrow(callback_play.key,49,a);
    owned.phase=P::Available;Check(ledger.Returned(original),"callback-era original return missing");
    Check(ledger.CheckWatch(callback_ticket,callback_play,a)==W::Current,"old return erased callback's playback");
    // This record can now be reused. Late duplicate completion/removal receipts
    // must not touch the next borrow (synthetic adverse ordering, not a claim
    // that the current ordinary synchronous game callback does this).
    const auto nested=ledger.Borrow(owned.key,50,a);owned.playback=50;owned.phase=P::Playing;
    Check(!ledger.Returned(original),"old completion retired reentrant borrow");
    Check(ledger.CheckWatch(*watch,owned,a)==W::Finished,"old wait followed new playback of same actor");
    Check(ledger.CheckWatch(nested,owned,a)==W::Current,"new borrow incorrectly finished");
    Check(!ledger.Removed(original),"old removal erased reentrant borrow");

    // Pool return is not field clearing; old IDs remain but private rows must
    // not become public simply because ownership bookkeeping was forgotten.
    owned.phase=P::Available;Check(ledger.Returned(nested),"actual return missing");
    Check(ledger.Visibility(owned,{})==V::Hidden,"returned private record became public");
    Check(ledger.CheckWatch(nested,owned,a)==W::Finished,"returned wait not complete");
    const auto next=ledger.Borrow(owned.key,51,b);owned.playback=51;owned.phase=P::Playing;
    Check(ledger.Visibility(owned,a)==V::Hidden && ledger.Visibility(owned,b)==V::Owned,"pool reuse kept old owner");
    Check(!ledger.Returned(nested),"stale return accepted after reuse");
    // Official borrow of this same record is a positive ownership transition.
    ledger.Borrow(owned.key,52,{});owned.playback=52;
    Check(ledger.Visibility(owned,{})==V::Public && ledger.Visibility(owned,b)==V::Hidden,"official reuse not restored");
    Check(ledger.CheckWatch(next,owned,b)==W::Finished,"old private waiter followed official borrow");
    const auto public_watch=ledger.Watch(owned,std::nullopt);
    Check(public_watch.has_value(),"official waiter has no reuse generation");
    ledger.Borrow(owned.key,53,a);owned.playback=53;
    Check(ledger.CheckWatch(*public_watch,owned,std::nullopt)==W::Finished,"official waiter followed private pool reuse");
    Check(!ledger.Watch(owned,std::nullopt),"official waiter adopted private playback");

    // Mismatch means missing observation, not a reason to expose the row.
    auto mismatch=another;mismatch.playback=900;
    const auto partial=ledger.Select({mismatch,official},"shared.playable",{});
    Check(Selected(partial,1) && partial.stale.size()==1,"independent official row lost on observation fault");
    Check(!ledger.Select({mismatch},"shared.playable",{}).record,"unobserved reuse leaked private row");
    const auto current=ledger.Watch(another,b);
    Check(current && ledger.CheckWatch(*current,mismatch,b)==W::StaleObservation,"unobserved reuse disguised as completion");
    auto wrong_record=Row(333,43,P::Playing);
    Check(ledger.CheckWatch(*current,wrong_record,b)==W::StaleObservation,"wrong record accepted by wait");

    // IDs and object addresses are not globally unique across registries.
    auto other_registry=Row(3,43,P::Playing,"shared.playable",11);
    const auto separate=ledger.Borrow(other_registry.key,43,a);
    ledger.RegistryDisposed(10);
    Check(ledger.Size()==1 && ledger.CheckWatch(separate,other_registry,a)==W::Current,"registry disposal crossed service boundary");
    Check(ledger.Removed(separate) && ledger.Size()==0,"actual removal not recorded");

    // Playback ID reuse/wrap does not resurrect a task's old ticket.
    const auto first=ledger.Borrow(owned.key,1,a);
    const auto repeated=ledger.Borrow(owned.key,1,a);owned.playback=1;
    Check(first.borrow!=repeated.borrow && ledger.CheckWatch(first,owned,a)==W::Finished,"ID reuse resurrected a wait");
    Check(ledger.CheckWatch(repeated,owned,a)==W::Current,"new matching ID incorrectly invalidated");

    // Repeated queries (WaitTimelineFinish.Tick) use the exact same scope;
    // clearing a task does not delete the playback. No task pointer is stored.
    for(int tick=0;tick<5;++tick)
        Check(Selected(ledger.Select({official,owned},"shared.playable",a),2),"subsequent wait query escaped scope");
    bool invalid=false;
    try{ledger.Borrow(owned.key,2,TimelineOwner{1,100,0});}catch(const std::invalid_argument&){invalid=true;}
    Check(invalid && ledger.CheckWatch(repeated,owned,a)==W::Current,"incomplete owner mutated current borrow");
    {
        TimelineCallScope own_call(a);
        Check(TimelineCallScope::Current()==a,"owned call scope missing");
        {
            TimelineCallScope official_call({});
            Check(!TimelineCallScope::Current(),"official reentry inherited private caller");
            TimelineCallScope other_call(b);
            Check(TimelineCallScope::Current()==b,"nested actor scope not replaced");
        }
        Check(TimelineCallScope::Current()==a,"nested call didn't restore caller");
        try{TimelineCallScope inner(b);throw std::runtime_error("unwind");}catch(const std::runtime_error&){}
        Check(TimelineCallScope::Current()==a,"C++ exception didn't restore caller");
        bool leaked=true;
        std::thread foreign([&]{leaked=TimelineCallScope::Current().has_value();});foreign.join();
        Check(!leaked,"owner scope leaked to another thread");
    }
    Check(!TimelineCallScope::Current(),"completed call leaked its owner");
    std::cout<<"Timeline ownership candidate: bidirectional selection, official preference, pool/callback reuse, wait generations, stale observations and registry boundaries passed. No game hooks installed.\n";
}
