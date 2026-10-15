#include "TimelineOwnership.hpp"
namespace hksr::sdk_v1::detail {
// One TLS stack in the SDK DLL, not one inline variable per consuming module.
// Native detours and candidate consumers must see the same scope stack.
thread_local TimelineCallScope* TimelineCallScope::current_=nullptr;
TimelineCallScope::TimelineCallScope(std::optional<TimelineOwner> owner,bool established)
    :previous_(current_),owner_(owner),established_(established){current_=this;}
TimelineCallScope::~TimelineCallScope(){current_=previous_;}
std::optional<TimelineOwner> TimelineCallScope::Current(){return current_ && current_->established_?current_->owner_:std::nullopt;}
bool TimelineCallScope::Established(){return current_ && current_->established_;}
bool TimelineCallScope::Active(){return current_!=nullptr;}
void TimelineCallScope::ExhaustResourceQueries(){if(current_)current_->resource_queries_=false;}
bool TimelineCallScope::ResourceQueriesAllowed(){return !current_ || current_->resource_queries_;}
}
