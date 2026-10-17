#include <hksr/presentation/timeline/OwnedTimeline.hpp>
#include "../../../src/presentation/timeline/TimelineBorrowCapture.hpp"
#include <stdexcept>

using namespace hksr::api;
namespace {
void Check(bool value){if(!value)throw std::runtime_error("owned Timeline test failed");}
}
void TestOwnedTimeline() {
    using namespace hksr::sdk_v1::detail;
    const TimelineOwner owner{1,2,3};
    TimelineOwnership ownership;
    const auto a=ownership.Borrow({5,6},7,owner);
    Check(ownership.Pending(a));
    const auto b=ownership.Borrow({5,6},8,owner);
    Check(!ownership.Pending(a) && ownership.Pending(b));
    Check(ownership.Returned(b) && !ownership.Pending(b));
    {
        TimelineBorrowCapture outer(owner);TimelineBorrowCapture::Entered(owner);
        {
            TimelineBorrowCapture inner(owner);TimelineBorrowCapture::Entered(owner);TimelineBorrowCapture::Record(b,owner);
            Check(inner.entered==1 && inner.tickets.size()==1);
        }
        TimelineBorrowCapture::Record(a,owner);
        TimelineBorrowCapture::Entered(std::nullopt);TimelineBorrowCapture::Record(b,std::nullopt);
        Check(outer.entered==1 && outer.tickets.size()==1 && outer.tickets[0].playback==7);
    }
    int starts=0,stops=0;
    TimelinePlaybackPhase phase=TimelinePlaybackPhase::Playing;
    TimelineRequestServices services;
    OwnedTimeline* active=nullptr;bool cancel_in_start=false,throw_stop=false;
    services.start=[&](const std::string& path,int wrap,double time) {
        ++starts;Check(path=="scene" && wrap==2 && time==0);
        if(cancel_in_start){active->Cancel();active->Service();}
        TimelineStartResult result;result.call.returned=true;result.playback=17;result.attributed=true;
        result.references.push_back({reinterpret_cast<void*>(5),reinterpret_cast<void*>(6),17,18});return result;
    };
    services.observe=[&](TimelineReference ref){Check(ref.playback==17 && ref.borrow==18);return phase;};
    services.stop=[&](TimelineReference){++stops;if(throw_stop)throw std::runtime_error("stop failure");phase=TimelinePlaybackPhase::Stopped;return CallResult{true,0};};
    OwnedTimeline request(services);active=&request;cancel_in_start=true;request.Start("scene",2);
    Check(request.Status().accepted && request.Status().cancelled && !request.Status().drained && stops==0);
    request.Service();Check(stops==1 && !request.Status().drained);
    request.Service();Check(stops==1 && request.Status().phases[0]==TimelinePlaybackPhase::Stopped && !request.Status().drained);
    phase=TimelinePlaybackPhase::Retired;request.Service();Check(request.Status().drained);
    OwnedTimeline pre_cancelled(services);pre_cancelled.Cancel();pre_cancelled.Start("scene",2);Check(starts==1 && pre_cancelled.Status().drained);
    cancel_in_start=false;phase=TimelinePlaybackPhase::Preparing;
    OwnedTimeline preparing(services);preparing.Start("scene",2);preparing.Cancel();preparing.Service();Check(stops==1 && !preparing.Status().drained);
    phase=TimelinePlaybackPhase::Playing;preparing.Service();Check(stops==2);
    phase=TimelinePlaybackPhase::Retired;preparing.Service();Check(preparing.Status().drained);
    phase=TimelinePlaybackPhase::Playing;throw_stop=true;
    OwnedTimeline failed(services);failed.Start("scene",2);failed.Cancel();failed.Service();failed.Service();Check(stops==3 && !failed.Status().drained && failed.Status().detail=="stop failure");
    // An interrupted submission can still have an attributed borrowed record.
    auto start=services.start;
    services.start=[&](const auto& path,int wrap,double time){auto result=start(path,wrap,time);result.call={false,123};return result;};
    throw_stop=false;OwnedTimeline partial(services);partial.Start("scene",2);partial.Service();
    Check(!partial.Status().accepted && partial.Status().cancelled && stops==4 && !partial.Status().drained);
    phase=TimelinePlaybackPhase::Retired;partial.Service();Check(partial.Status().drained);
    services.start=[](const std::string&,int,double){TimelineStartResult result;result.attributed=true;result.detail="source unavailable";return result;};
    OwnedTimeline no_submission(services);no_submission.Start("scene");Check(no_submission.Status().drained && !no_submission.Status().accepted);
}
