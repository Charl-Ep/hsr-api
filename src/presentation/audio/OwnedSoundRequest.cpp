#include <hksr/presentation/audio/OwnedSoundRequest.hpp>
#include <mutex>
#include <exception>
#include <utility>
#include <vector>

namespace hksr::sdk_v1 {
struct OwnedSoundRequest::State {
    struct Inbox {std::mutex mutex;std::vector<SoundCallbackEvent> events;};
    SoundRequestServices services;
    ManagedRoots roots;
    std::vector<ManagedRootId> retained;
    std::shared_ptr<Inbox> inbox=std::make_shared<Inbox>();
    std::unique_ptr<SoundCallbacks> callbacks;
    SoundRequestStatus status;
    void* manager=nullptr;
    void* emitter=nullptr;
    bool posted=false,stop_attempted=false,release=false,servicing=false;
    explicit State(SoundRequestServices value):services(std::move(value)),roots(services.references){}
    void Error(const char* text){if(status.error.empty())status.error=text;}
    bool Keep(void* value) {
        if(!value)return false;
        const auto id=roots.Keep(value);retained.push_back(id);return roots.Status(id).ready;
    }
    void Observe() {
        std::vector<SoundCallbackEvent> events;
        {std::lock_guard<std::mutex> lock(inbox->mutex);events.swap(inbox->events);}
        for(const auto& event:events) {
            // During a faulted submission a callback may be the only known
            // request identity. A successful return (including zero) wins.
            if(!status.submission.read.returned && !status.submission.value && event.logical_id)
                status.submission.value=event.logical_id;
            if(!event.logical_id || event.logical_id!=status.submission.value){++status.foreign_callbacks;continue;}
            if(event.kind==SoundCallbackKind::Began)status.started=true;
            else {status.ended=true;if(!event.has_info)Error("audio ended without playback information");}
        }
    }
    void Close() {
        const bool delegates=!callbacks || callbacks->Close();
        for(auto it=retained.begin();it!=retained.end();) {
            if(roots.Release(*it).complete){roots.Forget(*it);it=retained.erase(it);}else ++it;
        }
        status.closed=delegates && retained.empty();
        if(!status.closed)Error("audio reference release incomplete");
    }
};
OwnedSoundRequest::OwnedSoundRequest(SoundRequestServices services):state_(new State(std::move(services))){}
OwnedSoundRequest::~OwnedSoundRequest(){delete state_;}
void OwnedSoundRequest::Start(void* manager,void* name,void* emitter) {
    auto& s=*state_;if(s.status.attempted || s.status.cancelled || s.status.closed)return;
    s.status.attempted=true;
    try {
        if(s.services.current && !s.services.current()) {
            s.Error("audio owner unavailable");s.status.cancelled=true;s.release=true;
        } else if(!s.Keep(manager) || !s.Keep(name) || !s.Keep(emitter)) {
            s.Error("audio input retention failed");s.release=true;
        } else {
            s.manager=manager;s.emitter=emitter;
            const auto alive=s.services.access.EmitterAlive(emitter);
            if(!alive.read.returned || !alive.value){s.Error("audio emitter unavailable");s.release=true;}
            else {
                const auto inbox=s.inbox;
                s.callbacks=std::make_unique<SoundCallbacks>(s.services.callbacks,s.services.references,[inbox](SoundCallbackEvent event) {
                    std::lock_guard<std::mutex> lock(inbox->mutex);inbox->events.push_back(event);
                });
                const auto delegates=s.callbacks->Create();
                if(!delegates.began.call.returned || !delegates.began.object ||
                    !delegates.ended.call.returned || !delegates.ended.object) {
                    s.Error("audio callbacks unavailable");s.release=true;
                } else {
                    s.posted=true;
                    s.status.submission=s.services.access.Post(manager,name,emitter,delegates.ended.object,delegates.began.object);
                    s.posted=s.status.submission.attempted;
                    s.status.accepted=s.status.submission.read.returned && s.status.submission.value!=0;
                    if(!s.posted){s.Error("audio submission not attempted");s.release=true;}
                    else if(!s.status.submission.read.returned)s.Error("audio submission outcome unknown");
                    else if(!s.status.submission.value){s.Error("audio submission rejected");s.release=true;}
                }
            }
        }
    }catch(const std::exception& error){s.Error(error.what());if(!s.posted)s.release=true;}
    Service();
}
void OwnedSoundRequest::Cancel(){if(!state_->status.closed)state_->status.cancelled=true;}
void OwnedSoundRequest::Service(bool owner_current) {
    auto& s=*state_;if(s.status.closed || s.servicing)return;
    s.servicing=true;struct Guard{bool& value;~Guard(){value=false;}} guard{s.servicing};
    s.Observe();
    if(s.posted && !s.status.ended && !s.status.cancelled && s.services.current) {
        try{owner_current=owner_current && s.services.current();}
        catch(const std::exception& error){s.Error(error.what());owner_current=false;}
    }
    if(!owner_current)s.status.cancelled=true;
    if(s.posted && !s.status.ended && !s.status.cancelled && !s.release) {
        const auto alive=s.services.access.EmitterAlive(s.emitter);
        if(!alive.read.returned || !alive.value) {
            s.status.cancelled=true;s.Error("audio emitter lost");
        }
    }
    if(s.status.ended)s.release=true;
    if(s.status.cancelled && !s.release) {
        if(!s.posted)s.release=true;
        else if(s.status.submission.value && !s.stop_attempted) {
            s.stop_attempted=true;
            s.status.stop=s.services.access.Stop(s.manager,s.status.submission.value);
            s.Observe();
            if(s.status.stop.returned || s.status.ended)s.release=true;
            else s.Error("audio stop outcome unknown");
        }
    }
    if(s.release)s.Close();
}
const SoundRequestStatus& OwnedSoundRequest::Status() const{return state_->status;}
}
