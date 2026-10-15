#pragma once
#include <hksr/presentation/PresentationIsolation.hpp>
#include <map>
#include <mutex>
#include <tuple>
#include <stdexcept>

namespace hksr::sdk_v1::detail {
// One process service, not one set of global detours per SDK client. Failed
// construction is not published. The service survives the last client because
// official callbacks may still refer to its playback records.
template<class Service> class PresentationServiceSlot {
    std::mutex mutex_;
    std::shared_ptr<Service> service_;
public:
    template<class Factory> std::shared_ptr<Service> Acquire(Factory&& create) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!service_)service_=create();
        return service_;
    }
    std::shared_ptr<Service> Existing() {
        std::lock_guard<std::mutex> lock(mutex_);return service_;
    }
};

// Client callbacks can be registered on preparation threads and detached on
// shutdown. Origins contain world + entity generation, never just an address.
// Retired owners retain policy values but no client callback or DLL dependency.
class PresentationClients {
    using Key=std::tuple<std::uintptr_t,std::uintptr_t,std::uint64_t>;
    struct Client {PresentationIsolationPolicy policy;PresentationIsolation::Trace trace;};
    struct Source {std::uint64_t client;PresentationIsolationPolicy policy;};
    std::recursive_mutex mutex_;
    std::uint64_t next_=0;
    std::map<std::uint64_t,Client> clients_;
    std::map<Key,Source> sources_;
    static Key KeyOf(EntityOrigin o) {
        return {reinterpret_cast<std::uintptr_t>(o.world),reinterpret_cast<std::uintptr_t>(o.token.entity),o.token.generation};
    }
public:
    std::uint64_t Add(PresentationIsolationPolicy policy,PresentationIsolation::Trace trace) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        const auto id=++next_;clients_.emplace(id,Client{policy,std::move(trace)});return id;
    }
    void Detach(std::uint64_t id) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);clients_.erase(id);
    }
    void Remember(std::uint64_t id,EntityOrigin origin) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        const auto client=clients_.find(id);
        if(client==clients_.end())throw std::logic_error("presentation client retired");
        const auto result=sources_.emplace(KeyOf(origin),Source{id,client->second.policy});
        if(!result.second && result.first->second.client!=id)
            throw std::logic_error("presentation origin already belongs to another client");
    }
    PresentationIsolationPolicy Policy(EntityOrigin origin) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        const auto i=sources_.find(KeyOf(origin));
        if(i==sources_.end())throw std::logic_error("presentation owner policy unavailable");
        return i->second.policy;
    }
    void RequireOwner(std::uint64_t id,EntityOrigin origin) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        const auto i=sources_.find(KeyOf(origin));
        if(!clients_.count(id) || i==sources_.end() || i->second.client!=id)
            throw std::logic_error("presentation request belongs to another client");
    }
    template<class Alive> void Collect(Alive&& alive) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        for(auto i=sources_.begin();i!=sources_.end();) {
            const auto& [world,entity,generation]=i->first;
            const EntityOrigin origin{{reinterpret_cast<GameEntity*>(entity),generation},reinterpret_cast<GameWorld*>(world)};
            if(alive(origin))++i;else i=sources_.erase(i);
        }
    }
    void Log(const std::string& text) noexcept {
        try {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            // IDs permit callbacks to detach themselves without invalidating
            // iteration. No callback may survive Detach's return on another thread.
            std::vector<std::uint64_t> ids;
            for(const auto& c:clients_)ids.push_back(c.first);
            for(auto id:ids) {
                const auto i=clients_.find(id);if(i==clients_.end())continue;
                const auto trace=i->second.trace;
                if(trace)try{trace(text);}catch(...){}
            }
        }catch(...){}
    }
};
}
