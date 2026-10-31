#include "EntityEffectRetirement.hpp"
#include <stdexcept>

namespace hksr::sdk_v1::detail {
EntityEffectRetirement::EntityEffectRetirement(EffectRequestServices services,Alive alive)
    :services_(std::move(services)),alive_(std::move(alive)) {
    if(!alive_)throw std::invalid_argument("entity effect lifetime provider absent");
}
void EntityEffectRetirement::Before(const EffectSubmission& submission) {
    // After is paired even when tracking throws. No cancellation may run from
    // inside a producer, including synchronous completion or nested submission.
    calls_.insert(submission.serial);
    const auto& origin=submission.origin;
    if(!origin.token.entity || !origin.token.generation || !origin.world ||
        !submission.manager || !submission.request.parameters || !submission.request.id)
        throw std::invalid_argument("entity effect requires an attributed submission");
    const Key key{submission.manager,submission.request.parameters,submission.request.id};
    const auto previous=keys_.find(key);
    if(previous!=keys_.end()) {
        const auto& entry=entries_.at(previous->second);
        if(entry.origin.token.entity!=origin.token.entity || entry.origin.token.generation!=origin.token.generation || entry.origin.world!=origin.world)
            throw std::logic_error("same effect generation has conflicting entity ownership");
        return;
    }
    const auto id=++next_;
    auto request=std::make_unique<OwnedEffectRequest>(services_);
    auto& entry=entries_.emplace(id,Entry{key,origin,std::move(request)}).first->second;
    keys_.emplace(key,id);
    entry.retained=entry.request->TrackSubmitted(submission.manager,submission.request);
    if(!entry.retained)throw std::runtime_error("entity effect receipt retention incomplete");
}
void EntityEffectRetirement::After(const EffectSubmission& submission,bool) {
    // Failure can follow submission side effects. Keep the receipt and let
    // official release/pool generation, not the call result, determine its end.
    calls_.erase(submission.serial);
}
void EntityEffectRetirement::Collect() {
    if(collecting_ || !calls_.empty())return;
    collecting_=true;struct End{bool& value;~End(){value=false;}} end{collecting_};
    const auto last=next_;
    for(auto it=entries_.begin();it!=entries_.end() && it->first<=last;) {
        auto& entry=it->second;auto& request=*entry.request;
        try {
            if(entry.retained && !request.Status().cancelled && !alive_(entry.origin)) {
                request.Cancel(false);
                if(services_.trace)services_.trace("entity effect retirement generation="+std::to_string(entry.origin.token.generation)+
                    " effect="+std::to_string(request.Status().reference.id));
            }
            request.Service();
            if(request.Status().closed) {
                if(entry.retained && request.Status().cancelled && services_.trace)
                    services_.trace("entity effect retired generation="+std::to_string(entry.origin.token.generation)+
                        " effect="+std::to_string(request.Status().reference.id)+" detail="+request.Status().detail);
                // Tracking failures are diagnosed at admission; draining partial
                // roots is not a successful effect-retirement receipt.
                keys_.erase(entry.key);
                it=entries_.erase(it);continue;
            }
        }catch(const std::exception& error) {
            if(services_.trace)services_.trace(std::string("entity effect retirement: ")+error.what());
        }
        ++it;
    }
}
std::size_t EntityEffectRetirement::Pending() const{return entries_.size();}
}
