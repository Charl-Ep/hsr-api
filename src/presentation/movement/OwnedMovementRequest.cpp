#include <hksr/presentation/movement/OwnedMovementRequest.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct OwnedMovementRequest::State:std::enable_shared_from_this<State> {
    MovementRequestServices services;EntityToken owner;void* component;
    AnimationMovementData data;AnimationMovementAccess access;ManagedRoots roots;
    std::vector<ManagedRootId> retained;std::unique_ptr<AnimationMovementUpdates> updates;
    EntityDisposalSubscription disposal=0;MovementRequestStatus status;
    bool busy=false,submitted=false,clear_attempted=false,release=false;
    State(MovementRequestServices s,EntityToken o,void* c):services(std::move(s)),owner(o),component(c),
        data(services.data),access(services.access,o.entity),roots(services.data.references){}
    void Error(const std::string& value){if(status.error.empty())status.error=value;}
    void Keep(void* object) {
        retained.reserve(retained.size()+1);retained.push_back(roots.Keep(object));
        if(!roots.Status(retained.back()).ready)throw std::runtime_error("movement input retention failed");
    }
    void Disconnect() noexcept {
        try {if(disposal){services.lifecycle->CancelBeforeDispose(disposal);disposal=0;}}catch(...){}
        if(updates)updates->Close();
    }
    void Close() {
        Disconnect();
        const bool graph=data.Close();
        for(auto it=retained.begin();it!=retained.end();) {
            if(roots.Release(*it).complete){roots.Forget(*it);it=retained.erase(it);}else ++it;
        }
        status.closed=graph && retained.empty();
        if(!status.closed)Error("movement reference release incomplete");
    }
    void ObserveUpdates() {
        if(!updates)return;
        const auto value=updates->Status();
        status.started=status.started || value.started;
        status.completed=status.completed || value.completed;
        status.interrupted=status.interrupted || value.interrupted;
        status.replaced=status.replaced || value.replaced;
        status.animation_time=value.animation_time;status.movement_time=value.movement_time;
        if(value.failed)Error("movement evaluation outcome unknown");
    }
    void Service() {
        if(status.closed || busy)return;
        busy=true;struct Guard{bool& value;~Guard(){value=false;}} guard{busy};
        try {
            if(release){Close();return;}
            ObserveUpdates();
            if(!services.alive()) {
                status.owner_lost=true;status.cancelled=true;release=true;Close();return;
            }
            if(!services.current())status.cancelled=true;
            if(!submitted) {
                if(status.attempted || status.cancelled){release=true;Close();}
                return;
            }
            const auto observed=access.Observe(component);
            if(!observed.read.returned){Error("movement ownership unreadable");return;}
            const bool held=observed.data==data.Object();
            if(!held) {
                if(observed.data)status.replaced=true;
                if(!status.completed && !status.cancelled) {
                    status.interrupted=true;
                    Error(status.replaced?"movement replaced by another request":"movement detached without a completion receipt");
                }
                release=true;Close();return;
            }
            if(observed.owner!=owner.entity){Error("movement component owner changed");return;}
            if(status.completed){Error("completed movement graph is still attached");}
            if((status.cancelled || !status.error.empty()) && !clear_attempted) {
                clear_attempted=true;
                status.interruption=access.Interrupt(component,data,services.alive);
                const auto outcome=status.interruption.state;
                if(outcome==MovementWriteState::Detached || outcome==MovementWriteState::Replaced) {
                    status.replaced=status.replaced || outcome==MovementWriteState::Replaced;
                    release=true;Close();
                }else Error(status.interruption.error.empty()?"movement interruption outcome unknown":status.interruption.error);
            }
        }catch(const std::exception& e){Error(e.what());}
    }
};
OwnedMovementRequest::OwnedMovementRequest(MovementRequestServices services,EntityToken owner,void* component) {
    if(!owner.entity || !owner.generation || !component || !services.lifecycle || !services.current || !services.alive)
        throw std::invalid_argument("movement requires an owned entity lifetime and component");
    state_=std::make_shared<State>(std::move(services),owner,component);
}
OwnedMovementRequest::~OwnedMovementRequest(){state_->Disconnect();}
void OwnedMovementRequest::Start(const AnimationMovePlan& input) {
    auto& s=*state_;if(s.status.attempted || s.status.cancelled || s.status.closed || s.busy)return;
    s.status.attempted=true;s.busy=true;
    try {
        if(!s.services.alive() || !s.services.current())throw std::runtime_error("movement owner/model unavailable");
        s.Keep(s.owner.entity);s.Keep(s.component);
        for(auto* object:s.services.model_objects)if(object)s.Keep(object);
        if(!s.data.Prepare(input))throw std::runtime_error(s.data.Error());
        s.updates=std::make_unique<AnimationMovementUpdates>(s.services.update,s.services.access,s.owner.entity,s.component,s.data.Object());
        std::weak_ptr<State> weak=state_;
        s.disposal=s.services.lifecycle->BeforeDispose(s.owner,[weak](auto){
            if(auto state=weak.lock()){state->status.cancelled=true;state->Service();}
        });
        s.status.submission=s.access.Submit(s.component,s.data,[&s]{
            return !s.status.cancelled && s.services.alive() && s.services.current();
        });
        s.submitted=s.status.submission.attempted;
        s.status.accepted=s.status.submission.call.returned && s.status.submission.state==MovementWriteState::Owned;
        s.ObserveUpdates();
        // Very short requests may naturally finish inside Submit's zero-delta
        // evaluation. Only the evaluator receipt can establish this outcome.
        s.status.accepted=s.status.accepted || (s.status.submission.call.returned && s.status.completed);
        if(!s.submitted || s.status.submission.state==MovementWriteState::Failed)
            s.Error(s.status.submission.error.empty()?"movement submission failed":s.status.submission.error);
    }catch(const std::exception& e){s.Error(e.what());}
    if(!s.submitted)s.release=true;
    s.busy=false;s.Service();
}
void OwnedMovementRequest::Cancel(){if(!state_->status.closed)state_->status.cancelled=true;}
void OwnedMovementRequest::Service(){state_->Service();}
const MovementRequestStatus& OwnedMovementRequest::Status()const{return state_->status;}
}
