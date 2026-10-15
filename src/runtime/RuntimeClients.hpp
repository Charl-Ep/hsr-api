#pragma once
#include <hksr/runtime/RuntimeHost.hpp>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <stdexcept>

namespace hksr::sdk_v1::detail {
struct RuntimeClientRecord {
    std::atomic<bool> stop{false};
    mutable std::mutex mutex;
    RuntimeClientStatus status;
    // Accessed only by the host's serialized preparation/frame dispatcher.
    std::shared_ptr<RuntimeClient> client;
    explicit RuntimeClientRecord(std::shared_ptr<RuntimeClient> value):client(std::move(value)) {}
    RuntimeClientStatus Read() const {
        std::lock_guard<std::mutex> lock(mutex);auto out=status;
        out.stop_requested=stop.load();
        if(out.stop_requested && out.phase==RuntimeHostPhase::Running)out.phase=RuntimeHostPhase::Stopping;
        return out;
    }
    void Finish(RuntimeHostPhase phase,const char* error=nullptr) {
        client.reset(); // Release before publishing completion, outside status lock.
        std::lock_guard<std::mutex> lock(mutex);
        status.phase=phase;if(error)status.preparation_error=error;
    }
};

// Synchronizes registration only. Prepare/Deliver/Fail are serialized by the
// host; callbacks run without the registry or status mutex. Snapshots make
// registration from inside a callback take effect at the next boundary.
class RuntimeClients {
    std::mutex mutex_;
    std::vector<std::shared_ptr<RuntimeClientRecord>> records_;
    std::vector<std::shared_ptr<RuntimeClientRecord>> Snapshot() {
        std::lock_guard<std::mutex> lock(mutex_);return records_;
    }
    void Prune() {
        std::lock_guard<std::mutex> lock(mutex_);
        records_.erase(std::remove_if(records_.begin(),records_.end(),[](const auto& record){
            const auto phase=record->Read().phase;
            return phase==RuntimeHostPhase::Stopped || phase==RuntimeHostPhase::Failed;
        }),records_.end());
    }
public:
    bool HasPending() {
        for(const auto& record:Snapshot())if(record->Read().phase==RuntimeHostPhase::Starting)return true;
        return false;
    }
    std::shared_ptr<RuntimeClientRecord> Add(std::shared_ptr<RuntimeClient> client) {
        if(!client)throw std::invalid_argument("runtime registration requires a client");
        auto record=std::make_shared<RuntimeClientRecord>(std::move(client));
        std::lock_guard<std::mutex> lock(mutex_);records_.push_back(record);return record;
    }
    template<class Prepare> void PreparePending(Prepare&& prepare) {
        for(const auto& record:Snapshot()) {
            if(record->Read().phase!=RuntimeHostPhase::Starting)continue;
            if(record->stop.load()) {record->Finish(RuntimeHostPhase::Stopped);continue;}
            try {
                prepare(*record->client);
                std::lock_guard<std::mutex> lock(record->mutex);
                record->status.prepared=true;
                record->status.phase=record->stop.load()?RuntimeHostPhase::Stopping:RuntimeHostPhase::Running;
            }catch(const std::exception& e){record->Finish(RuntimeHostPhase::Failed,e.what());}
            catch(...){record->Finish(RuntimeHostPhase::Failed,"unknown client preparation failure");}
        }
        Prune();
    }
    // Infrastructure failure affects pending registrations, not already-running
    // clients. A failed late worker attachment must not stop existing owners.
    void FailPending(const char* error) {
        for(const auto& record:Snapshot())if(record->Read().phase==RuntimeHostPhase::Starting)
            record->Finish(record->stop.load()?RuntimeHostPhase::Stopped:RuntimeHostPhase::Failed,error);
        Prune();
    }
    void FailStartup(const char* error) {
        for(const auto& record:Snapshot())
            record->Finish(record->stop.load()?RuntimeHostPhase::Stopped:RuntimeHostPhase::Failed,error);
        Prune();
    }
    template<class Report> void Deliver(const WorldFrame& frame,Report&& report) {
        struct Delivery {
            std::shared_ptr<RuntimeClientRecord> record;
            bool stopping=false,done=false;
        };
        std::vector<Delivery> batch;
        for(const auto& record:Snapshot()) {
            const auto phase=record->Read().phase;
            if(phase!=RuntimeHostPhase::Running && phase!=RuntimeHostPhase::Stopping)continue;
            batch.push_back({record,record->stop.load(),false});
        }
        const auto invoke=[&](const auto& record,auto&& operation) {
            try {operation();return true;}
            catch(const std::exception& e) {
                {std::lock_guard<std::mutex> lock(record->mutex);
                    ++record->status.frame_failures;record->status.last_frame_error=e.what();}
                report(e.what());
            }catch(...) {
                {std::lock_guard<std::mutex> lock(record->mutex);
                    ++record->status.frame_failures;record->status.last_frame_error="unknown client frame failure";}
                report("unknown client frame failure");
            }
            return false;
        };
        for(auto& item:batch) {
            const auto& record=item.record;
            {
                std::lock_guard<std::mutex> lock(record->mutex);
                ++record->status.frames;
                if(item.stopping)record->status.phase=RuntimeHostPhase::Stopping;
            }
            invoke(record,[&]{record->client->BeforeFrame(frame,item.stopping);});
        }
        for(auto& item:batch)
            invoke(item.record,[&]{item.done=item.record->client->Frame(frame,item.stopping);});
        for(auto i=batch.rbegin();i!=batch.rend();++i) {
            // Do not announce completion before the final observer cleanup.
            // If cleanup throws, retain this client for the next stop frame.
            const auto finished=invoke(i->record,[&]{i->record->client->AfterFrame(frame,i->stopping);});
            if(i->done && finished)i->record->Finish(RuntimeHostPhase::Stopped);
        }
        Prune();
    }
};
}
