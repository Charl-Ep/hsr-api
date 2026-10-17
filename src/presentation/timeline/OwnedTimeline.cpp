#include <hksr/presentation/timeline/OwnedTimeline.hpp>
#include <cmath>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct OwnedTimeline::State {
    TimelineRequestServices services;
    TimelineRequestStatus status;
    std::vector<bool> attempted;
    bool busy=false;
    explicit State(TimelineRequestServices value):services(std::move(value)){}
};
OwnedTimeline::OwnedTimeline(TimelineRequestServices value):state_(new State(std::move(value))){}
OwnedTimeline::~OwnedTimeline(){delete state_;}
const TimelineRequestStatus& OwnedTimeline::Status() const{return state_->status;}
void OwnedTimeline::Cancel(){state_->status.cancelled=true;}
void OwnedTimeline::Start(const std::string& path,int wrap,double time) {
    auto& s=*state_;
    if(s.status.started || s.busy)throw std::logic_error("Timeline request already started");
    if(path.empty() || path.find('\0')!=std::string::npos || wrap<0 || wrap>2 || !std::isfinite(time) || time<0)
        throw std::invalid_argument("invalid Common Timeline input");
    if(!s.services.start || !s.services.observe || !s.services.stop)throw std::invalid_argument("incomplete Timeline services");
    const auto copy=path; // providers may reenter and replace caller-owned input
    s.status.started=true;
    if(s.status.cancelled){s.status.drained=true;return;}
    s.busy=true;
    try {
        s.status.start=s.services.start(copy,wrap,time);
        s.status.accepted=s.status.start.call.returned && s.status.start.attributed;
        s.status.detail=s.status.start.detail;
        const auto count=s.status.start.references.size();
        s.status.phases.assign(count,TimelinePlaybackPhase::Unresolved);
        s.status.stops.resize(count);s.attempted.assign(count,false);
        s.status.drained=s.status.start.attributed && count==0;
        if(!s.status.accepted)s.status.cancelled=true;
    }catch(const std::exception& e){s.status.detail=e.what();s.status.cancelled=true;}
    s.busy=false;
}
void OwnedTimeline::Service() {
    auto& s=*state_;
    if(s.busy || !s.status.started || s.status.drained)return;
    s.busy=true;
    bool drained=s.status.start.attributed;
    for(std::size_t i=0;i<s.status.start.references.size();++i) {
        const auto ref=s.status.start.references[i];
        try {
            auto phase=s.services.observe(ref);s.status.phases[i]=phase;
            if(phase==TimelinePlaybackPhase::Retired)continue;
            drained=false;
            // Preparing may still be inside a reentrant native start. The
            // official record stop is effective only in Playing/Paused.
            if(s.status.cancelled && !s.attempted[i] &&
                (phase==TimelinePlaybackPhase::Playing || phase==TimelinePlaybackPhase::Paused)) {
                s.attempted[i]=true; // uncertain writes are not automatically retried
                s.status.stops[i]=s.services.stop(ref);
                if(!s.status.stops[i].returned)s.status.detail="Timeline stop did not return normally";
            }
        }catch(const std::exception& e){s.status.detail=e.what();drained=false;}
    }
    s.status.drained=drained;s.busy=false;
}
}
