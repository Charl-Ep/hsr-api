#include <hksr/presentation/movement/OwnedMovementHost.hpp>
#include <stdexcept>
#include <sstream>
#include <chrono>

namespace hksr::sdk_v1 {
struct OwnedMovementHost::State {
    MovementHostServices calls;EntityToken owner;ManagedRoots roots;
    std::vector<ManagedRootId> held;void* component=nullptr;
    MovementHostStatus status;
    bool busy=false,closing=false,root_changed=false,callback_changed=false,creation_attempted=false;
    bool root_before=false,callback_before=false,remove_attempted=false;
    bool root_restore_attempted=false,callback_restore_attempted=false;
    State(MovementHostServices s,EntityToken t):calls(std::move(s)),owner(t),roots(calls.references){}
    void Log(const char* event,const std::string& detail={})noexcept {
        if(calls.trace)try{std::ostringstream out;out<<"movement-host event="<<event<<" entity="<<owner.entity
            <<" generation="<<owner.generation<<" component="<<component<<" created="<<status.created
            <<" root_before="<<root_before<<" callback_before="<<callback_before;
            if(!detail.empty())out<<" detail=["<<detail<<"]";calls.trace(out.str());}catch(...){}
    }
    void Error(const std::string& text){if(status.error.empty()){status.error=text;Log("failed",text);}}
    void Keep(void* value) {
        held.reserve(held.size()+1);held.push_back(roots.Keep(value));
        if(!roots.Status(held.back()).ready)throw std::runtime_error("movement host retention failed");
    }
    bool Empty() {
        const auto value=calls.observe(component);
        if(!value.read.returned || value.owner!=owner.entity)throw std::runtime_error("movement host owner unreadable or changed");
        return !value.data && !value.other_motion && !value.attached;
    }
    void Restore() {
        if(callback_changed && !callback_restore_attempted) {
            callback_restore_attempted=true;
            status.callback_restore=calls.write_callback(component,callback_before);
            if(!status.callback_restore.returned)Error("movement callback restore failed");
        }
        if(root_changed && !root_restore_attempted) {
            root_restore_attempted=true;status.root_restore=calls.write_root_motion(root_before);
            if(!status.root_restore.returned)Error("original root motion restore failed");
        }
    }
    bool Release() {
        for(auto it=held.begin();it!=held.end();) {
            if(roots.Release(*it).complete){roots.Forget(*it);it=held.erase(it);}else ++it;
        }
        return held.empty();
    }
};
OwnedMovementHost::OwnedMovementHost(MovementHostServices services,EntityToken token):state_(nullptr) {
    if(!token.entity || !token.generation || !services.current || !services.alive || !services.lookup ||
        !services.create || !services.initialize || !services.remove || !services.observe ||
        !services.read_root_motion || !services.write_root_motion || !services.read_pending_motion ||
        !services.read_callback || !services.write_callback)
        throw std::invalid_argument("movement host requires complete entity and control services");
    state_=new State(std::move(services),token);
}
OwnedMovementHost::~OwnedMovementHost(){delete state_;}
bool OwnedMovementHost::Prepare() {
    auto& s=*state_;
    if(s.busy || s.closing || s.status.closed)return false;
    if(s.status.attempted)return s.status.prepared;
    s.status.attempted=true;s.busy=true;
    const auto begin=std::chrono::steady_clock::now();
    try {
        if(!s.calls.alive() || !s.calls.current())throw std::runtime_error("movement host entity/model unavailable");
        s.Keep(s.owner.entity);for(auto* value:s.calls.retained_inputs)if(value)s.Keep(value);
        const auto existing=s.calls.lookup();
        if(!existing.call.returned)throw std::runtime_error("movement component lookup failed");
        s.component=existing.object;
        if(!s.component) {
            // Even an exception can leave a registered component. Read the
            // postcondition and roll it back in this same transaction.
            s.creation_attempted=true;
            const auto made=s.calls.create();s.status.creation=made.call;
            const auto after=s.calls.lookup();
            if(!after.call.returned)throw std::runtime_error("component creation outcome unreadable");
            s.component=after.object;s.status.created=s.component!=nullptr;
            if(s.component)s.Keep(s.component);
            if(!made.call.returned || !made.object || made.object!=s.component)
                throw std::runtime_error("component creation did not return its registration");
            s.status.initialization=s.calls.initialize(s.component);
            if(!s.status.initialization.returned)throw std::runtime_error("movement component initialization failed");
        }else s.Keep(s.component);
        if(!s.calls.alive() || !s.calls.current())throw std::runtime_error("movement owner changed during initialization");
        const auto ready=s.calls.observe(s.component);
        if(!ready.read.returned || ready.owner!=s.owner.entity || !ready.transform || !ready.animator_source)
            throw std::runtime_error("movement component is not initialized on this entity");
        if(!s.Empty())throw std::runtime_error("movement host has an active motion channel");
        const auto root=s.calls.read_root_motion(),callback=s.calls.read_callback(s.component);
        if(!root.read.returned || !callback.read.returned)throw std::runtime_error("movement policy unreadable");
        s.root_before=root.value;s.callback_before=callback.value;
        // Flags record attempted writes as well as normal returns, so failure
        // still restores original policy during immediate rollback.
        if(callback.value) {
            s.callback_changed=true;s.status.callback_pause=s.calls.write_callback(s.component,false);
            if(!s.status.callback_pause.returned)throw std::runtime_error("movement root callback pause failed");
        }
        if(root.value) {
            s.root_changed=true;s.status.root_pause=s.calls.write_root_motion(false);
            if(!s.status.root_pause.returned)throw std::runtime_error("Adventure root motion pause failed");
        }
        s.status.prepared=true;
    }catch(const std::exception& e){s.Error(e.what());}
    s.busy=false;
    s.Log(s.status.prepared?"prepared":"prepare-failed","elapsed_us="+std::to_string(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count()));
    if(!s.status.prepared)Close();
    return s.status.prepared;
}
bool OwnedMovementHost::Ready() {
    auto& s=*state_;const bool waiting=s.status.waiting;s.status.waiting=false;
    if(!s.status.prepared || s.closing || s.status.closed)return false;
    try {
        if(!s.calls.alive() || !s.calls.current())throw std::runtime_error("movement owner/model changed");
        const auto root=s.calls.read_root_motion(),callback=s.calls.read_callback(s.component);
        if(!root.read.returned || !callback.read.returned || root.value || callback.value)
            throw std::runtime_error("movement control policy changed outside this lease");
        const auto pending=s.calls.read_pending_motion();
        if(!pending.read.returned)throw std::runtime_error("pending Adventure movement unreadable");
        s.status.waiting=pending.value;if(waiting!=pending.value)s.Log(pending.value?"waiting-for-motion":"motion-drained");return !pending.value;
    }catch(const std::exception& e){s.Error(e.what());return false;}
}
bool OwnedMovementHost::Close() {
    auto& s=*state_;if(s.status.closed)return true;if(s.busy)return false;
    s.closing=true;s.busy=true;s.status.waiting=false;
    try {
        if(!s.calls.alive())s.status.owner_lost=true;
        else {
            if(s.creation_attempted && !s.component) {
                const auto found=s.calls.lookup();
                if(!found.call.returned)throw std::runtime_error("component creation rollback lookup failed");
                if(found.object){s.component=found.object;s.status.created=true;s.Keep(s.component);}
            }
            if(s.component) {
                const auto now=s.calls.lookup();
                if(!now.call.returned)throw std::runtime_error("movement component cleanup lookup failed");
                if(now.object!=s.component) {
                    s.status.component_replaced=true;
                    // Do not write the old component or dispose its replacement.
                    s.callback_changed=false;
                }else {
                    if(s.status.prepared && !s.Empty())throw std::runtime_error("movement host still has active channels");
                    if(s.status.created) {
                        if(!s.remove_attempted) {
                            s.remove_attempted=true;s.status.removal=s.calls.remove(s.component);
                        }
                        if(!s.status.removal.returned)throw std::runtime_error("movement component removal outcome unknown");
                        const auto removed=s.calls.lookup();
                        if(!removed.call.returned || removed.object==s.component)
                            throw std::runtime_error("movement component removal not observed");
                        s.callback_changed=false; // registered callback ended with component
                    }
                }
                s.Restore();
                if((s.root_changed && !s.status.root_restore.returned) ||
                    (s.callback_changed && !s.status.callback_restore.returned))
                    throw std::runtime_error("movement policy cleanup incomplete");
            }
        }
        s.status.closed=s.Release();
        if(s.status.closed)s.Log("closed",s.status.owner_lost?"owner ended":s.status.component_replaced?"component replaced":"policy restored");
        if(!s.status.closed)s.Error("movement host roots remain retained");
    }catch(const std::exception& e){s.Error(e.what());}
    s.busy=false;return s.status.closed;
}
void* OwnedMovementHost::Component()const{return state_->status.prepared && !state_->status.closed?state_->component:nullptr;}
const MovementHostStatus& OwnedMovementHost::Status()const{return state_->status;}
}
