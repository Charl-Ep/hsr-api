#pragma once
#include "TimelineOwnership.hpp"

namespace hksr::sdk_v1::detail {
// Stack-local attribution for an explicit synchronous Common submission.
// Nested submissions get independent receipts, including same-owner requests.
class TimelineBorrowCapture {
public:
    explicit TimelineBorrowCapture(TimelineOwner owner):previous_(current_),owner_(owner){current_=this;}
    ~TimelineBorrowCapture(){current_=previous_;}
    TimelineBorrowCapture(const TimelineBorrowCapture&)=delete;
    TimelineBorrowCapture& operator=(const TimelineBorrowCapture&)=delete;
    static void Entered(std::optional<TimelineOwner> owner) {
        if(current_ && owner==current_->owner_)++current_->entered;
    }
    static void Record(TimelineTicket ticket,std::optional<TimelineOwner> owner) {
        if(current_ && owner==current_->owner_)current_->tickets.push_back(ticket);
    }
    std::vector<TimelineTicket> tickets;
    std::size_t entered=0;
private:
    inline static thread_local TimelineBorrowCapture* current_=nullptr;
    TimelineBorrowCapture* previous_;
    TimelineOwner owner_;
};
}
