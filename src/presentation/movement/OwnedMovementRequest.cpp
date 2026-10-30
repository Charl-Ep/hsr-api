#include <hksr/presentation/movement/OwnedMovementRequest.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <stdexcept>
#include <optional>

namespace hksr::sdk_v1 {
struct OwnedMovementRequest::State:std::enable_shared_from_this<State> {
    MovementRequestServices services;EntityToken owner;void* component;
    std::unique_ptr<AnimationMovementData> data;AnimationMovementAccess access;ManagedRoots roots;
    std::vector<ManagedRootId> retained;std::unique_ptr<AnimationMovementUpdates> updates;
    EntityDisposalSubscription disposal=0;MovementRequestStatus status;
    bool busy=false,submitted=false,clear_attempted=false,release=false;
    std::optional<AnimationMovePlan> pending;
    std::optional<Position3> pending_position;
    State(MovementRequestServices s,EntityToken o,void* c):services(std::move(s)),owner(o),component(c),
        access(services.access,o.entity),roots(services.data.references){}
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
        status.waiting_for_host=false;
        Disconnect();
        if(services.host && !services.host->Close()) {
            Error(services.host->Status().error.empty()?"movement host cleanup pending":services.host->Status().error);return;
        }
        const bool graph=!data || data->Close();
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
    void Submit() {
        if(!pending && !pending_position)return;
        if(services.host && !services.host->Ready()) {
            status.waiting_for_host=services.host->Status().waiting;
            if(!services.host->Status().error.empty())throw std::runtime_error(services.host->Status().error);
            if(!status.waiting_for_host)throw std::runtime_error("movement host no longer available");
            return;
        }
        status.waiting_for_host=false;
        if(pending_position) {
            const auto position=*pending_position;pending_position.reset();
            status.submission=access.Position(component,position,[this]{return !status.cancelled && services.alive() && services.current();});
            status.accepted=status.submission.call.returned && status.submission.state==MovementWriteState::Detached;
            status.started=status.completed=status.accepted;
            if(!status.accepted)Error(status.submission.error.empty()?"movement positioning failed":status.submission.error);
            release=true;Close();return;
        }
        if(services.origin)pending->origin=services.origin();
        data=std::make_unique<AnimationMovementData>(services.data);
        if(!data->Prepare(*pending))throw std::runtime_error(data->Error());
        pending.reset();
        updates=std::make_unique<AnimationMovementUpdates>(services.update,services.access,owner.entity,component,data->Object());
        status.submission=access.Submit(component,*data,[this]{return !status.cancelled && services.alive() && services.current();});
        submitted=status.submission.attempted;
        status.accepted=status.submission.call.returned && status.submission.state==MovementWriteState::Owned;
        ObserveUpdates();
        status.accepted=status.accepted || (status.submission.call.returned && status.completed);
        if(!submitted || status.submission.state==MovementWriteState::Failed)
            Error(status.submission.error.empty()?"movement submission failed":status.submission.error);
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
            if((pending || pending_position) && !status.cancelled && status.error.empty())Submit();
            if(release)return;
            if(!submitted) {
                if((pending || pending_position) && !status.cancelled && status.error.empty())return;
                if(status.attempted || status.cancelled){release=true;Close();}
                return;
            }
            const auto observed=access.Observe(component);
            if(!observed.read.returned){Error("movement ownership unreadable");return;}
            const bool held=observed.data==data->Object();
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
                status.interruption=access.Interrupt(component,*data,services.alive);
                const auto outcome=status.interruption.state;
                if(outcome==MovementWriteState::Detached || outcome==MovementWriteState::Replaced) {
                    status.replaced=status.replaced || outcome==MovementWriteState::Replaced;
                    release=true;Close();
                }else Error(status.interruption.error.empty()?"movement interruption outcome unknown":status.interruption.error);
            }
        }catch(const std::exception& e){Error(e.what());if(!submitted){release=true;Close();}}
    }
};
OwnedMovementRequest::OwnedMovementRequest(MovementRequestServices services,EntityToken owner,void* component) {
    if(!owner.entity || !owner.generation || !component || !services.lifecycle || !services.current || !services.alive)
        throw std::invalid_argument("movement requires an owned entity lifetime and component");
    state_=std::make_shared<State>(std::move(services),owner,component);
}
OwnedMovementRequest::~OwnedMovementRequest(){state_->Disconnect();}
void OwnedMovementRequest::Start(const AnimationMovePlan& input) {
    if(state_->status.attempted || state_->status.cancelled || state_->status.closed || state_->busy)return;
    state_->pending=input;Begin();
}
void OwnedMovementRequest::StartPosition(Position3 input) {
    if(state_->status.attempted || state_->status.cancelled || state_->status.closed || state_->busy)return;
    state_->pending_position=input;Begin();
}
void OwnedMovementRequest::Begin() {
    auto& s=*state_;if(s.status.attempted || s.status.cancelled || s.status.closed || s.busy)return;
    s.status.attempted=true;s.busy=true;
    try {
        if(!s.services.alive() || !s.services.current())throw std::runtime_error("movement owner/model unavailable");
        s.Keep(s.owner.entity);s.Keep(s.component);
        for(auto* object:s.services.model_objects)if(object)s.Keep(object);
        std::weak_ptr<State> weak=state_;
        s.disposal=s.services.lifecycle->BeforeDispose(s.owner,[weak](auto){
            if(auto state=weak.lock()){state->status.cancelled=true;state->Service();}
        });
    }catch(const std::exception& e){s.Error(e.what());}
    if(!s.status.error.empty())s.release=true;
    s.busy=false;s.Service();
}
void OwnedMovementRequest::Cancel(){if(!state_->status.closed)state_->status.cancelled=true;}
void OwnedMovementRequest::Service(){state_->Service();}
const MovementRequestStatus& OwnedMovementRequest::Status()const{return state_->status;}
}
