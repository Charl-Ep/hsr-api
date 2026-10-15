#include <hksr/runtime/RuntimeHost.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/model/ModelRelease.hpp>
#include <hksr/presentation/timeline/EventRecordLifetime.hpp>
#include <hksr/runtime/RuntimeAvailability.hpp>
#include "RuntimeStartup.hpp"
#include "../../adapters/runtime/RuntimeProfile.hpp"
#include "RuntimeResident.hpp"
#include "RuntimeClients.hpp"
#include <Windows.h>
#include <atomic>
#include <mutex>
#include <stdexcept>

namespace hksr::sdk_v1 {
namespace {
struct Host {
    RuntimeVersion version=RuntimeVersion::Hsr460;
    std::mutex status_mutex,delivery_mutex;
    RuntimeHostStatus status;
    std::atomic<bool> enabled{false};
    detail::RuntimeClients clients;
    HANDLE wake=nullptr;
    std::shared_ptr<RuntimeMetadata> metadata;
    std::unique_ptr<RuntimeThread> attachment;
    WorldFrameMethods frames;
    // The shared infrastructure outlives subscriptions. Cancellation belongs
    // to each record, not to whichever DLL happened to start the host first.
    bool Stopping() const {return false;}
    void Error(const char* error) noexcept {
        try {std::lock_guard<std::mutex> lock(status_mutex);status.startup_errors.emplace_back(error);}
        catch(...) {} // an allocation failure cannot escape the thread procedure
    }
    void Enter() {
        if(attachment && !attachment->Status().closed)
            throw std::runtime_error("previous worker attachment was not cleanly closed; not attaching again");
        const auto availability=ObserveRuntimeAvailability(version);
        if(availability.state!=RuntimeAvailability::Available)
            throw std::runtime_error("runtime initialization has not completed: "+availability.error);
        attachment=std::make_unique<RuntimeThread>(ResolveRuntimeThreadMethods(version));
        if(!attachment->Enter().ready) throw std::runtime_error(attachment->Status().error);
    }
    void OpenMetadata() {metadata=RuntimeMetadata::Open(version);}
    void InstallObservers() {
        RuntimeCapabilities caps;
        auto install=[this](auto operation,bool& available) {
            try {operation(*metadata);available=true;}
            catch(const std::exception& e) {Error(e.what());}
            catch(...) {Error("unknown observer installation failure");}
        };
        install(InstallEntityLifecycleHooks,caps.entity_lifecycle);
        install(InstallModelReleaseHooks,caps.model_release);
        install(InstallEventRecordLifetime,caps.event_record_lifetime);
        std::lock_guard<std::mutex> lock(status_mutex);status.capabilities=caps;
    }
    void PrepareClient() {
        RuntimeCapabilities caps;
        {std::lock_guard<std::mutex> lock(status_mutex);caps=status.capabilities;}
        clients.PreparePending([&](RuntimeClient& client){client.Prepare(*metadata,caps);});
        // Missing phase observation is reported independently, not allowed to
        // prevent delivery of disposing worlds or cleanup of existing owners.
        std::vector<DeclaredField> fields;
        try {fields=metadata->ReadFields("RPG.GameCore","GameWorld");}
        catch(const std::exception& e) {Error(e.what());}
        frames=ResolveWorldFrameMethods(*metadata,fields);
        for(const auto& error:frames.binding_errors) Error(error.c_str());
    }
    static void FailedFrame(const char* error,void* context) noexcept {
        auto& self=*static_cast<Host*>(context);
        try {
            std::lock_guard<std::mutex> lock(self.status_mutex);
            ++self.status.frame_failures;self.status.last_frame_error=error;
        } catch(...) {}
    }
    static void Deliver(const WorldFrame& frame,void* context) {
        auto& self=*static_cast<Host*>(context);
        // Never block an official update behind UI or another world callback.
        // This lock protects client lifetime, not the original engine update.
        std::unique_lock<std::mutex> delivery(self.delivery_mutex,std::try_to_lock);
        if(!delivery.owns_lock()) {
            std::lock_guard<std::mutex> lock(self.status_mutex);++self.status.skipped_frames;return;
        }
        detail::ServiceRetiredPresentations();
        if(!self.enabled.load()) return;
        {
            std::lock_guard<std::mutex> lock(self.status_mutex);
            if(!self.status.consumer_thread) self.status.consumer_thread=frame.thread_id;
            if(self.status.consumer_thread!=frame.thread_id) {
                ++self.status.skipped_frames;
                self.status.last_frame_error="world callback changed OS thread; client was not migrated";
                return;
            }
            ++self.status.frames;
        }
        // One failed observer cannot suppress another client's cleanup.
        self.clients.Deliver(frame,[&](const char* error){FailedFrame(error,&self);});
    }
    void InstallFrames() {
        InstallWorldFrameHook(frames,{Deliver,FailedFrame,this});
        std::lock_guard<std::mutex> lock(status_mutex);status.world_hook=true;
    }
    void Leave() {
        if(!attachment) return;
        const auto result=attachment->Leave();
        {std::lock_guard<std::mutex> lock(status_mutex);status.attachment=result;}
        if(!result.closed) Error(result.error.c_str());
    }
    void Finish(bool prepared,bool installed) noexcept {
        // Prepare is prohibited from creating game objects; a cancelled or
        // failed startup has no framework game ownership to drain.
        {
            std::lock_guard<std::mutex> lock(status_mutex);
            status.startup_finished=true;
            status.phase=installed && prepared?RuntimeHostPhase::Running:RuntimeHostPhase::Failed;
        }
        // Publish failure before taking the final registration snapshot. New
        // registrations must not slip behind the failed startup's drain.
        if(!installed || !prepared) clients.FailStartup("SDK host initialization failed; see host startup_errors");
        if(installed && prepared) enabled.store(true);
    }
    void PrepareLater() noexcept {
        // SDK metadata caches are mutable. A late client's preparation is
        // serialized with frames; the game callback uses try_lock and returns
        // to the engine immediately, never waits for this worker.
        std::lock_guard<std::mutex> delivery(delivery_mutex);
        if(!clients.HasPending())return;
        try {
            Enter();
            RuntimeCapabilities caps;
            {std::lock_guard<std::mutex> lock(status_mutex);caps=status.capabilities;}
            clients.PreparePending([&](RuntimeClient& client){client.Prepare(*metadata,caps);});
        }catch(const std::exception& e){Error(e.what());clients.FailPending(e.what());}
        catch(...){Error("late preparation failed");clients.FailPending("late preparation failed");}
        try {Leave();}catch(const std::exception& e){Error(e.what());}
        catch(...){Error("late worker detach failed");}
    }
};
// Process lifetime is deliberate: no static-destructor game calls, joining a
// dead world thread, freeing live trampolines, or DLL detach cleanup.
std::mutex& StartMutex() {static auto* value=new std::mutex;return *value;}
Host*& Instance() {static Host* value=nullptr;return value;}
DWORD WINAPI Worker(void* data) {
    auto& host=*static_cast<Host*>(data);
    {std::lock_guard<std::mutex> lock(host.status_mutex);host.status.worker_thread=GetCurrentThreadId();}
    {
        std::lock_guard<std::mutex> delivery(host.delivery_mutex);
        detail::RunRuntimeStartup(host);
    }
    // Idle worker holds no runtime attachment. No polling or game auto-start.
    if(host.enabled.load())
        while(WaitForSingleObject(host.wake,INFINITE)==WAIT_OBJECT_0)host.PrepareLater();
    {std::lock_guard<std::mutex> lock(host.status_mutex);host.status.worker_finished=true;}
    return 0;
}
}
namespace {
std::shared_ptr<detail::RuntimeClientRecord> Register(std::shared_ptr<RuntimeClient> client,RuntimeVersion version) {
    if(!client) throw std::invalid_argument("runtime host requires a client");
    (void)detail::RuntimeProfileFor(version);
    std::lock_guard<std::mutex> lock(StartMutex());
    if(auto* host=Instance()) {
        if(host->version!=version)throw std::logic_error("runtime host already uses a different game profile");
        std::lock_guard<std::mutex> status(host->status_mutex);
        if(host->status.phase==RuntimeHostPhase::Failed)throw std::logic_error("runtime host initialization failed; not restarted");
        auto record=host->clients.Add(std::move(client));SetEvent(host->wake);return record;
    }
    auto host=std::make_unique<Host>();host->version=version;
    auto record=host->clients.Add(std::move(client));
    HMODULE module=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&Worker),&module)) throw std::runtime_error("cannot retain SDK code for worker/hooks");
    host->status.resident=true;
    host->wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!host->wake)throw std::runtime_error("cannot create runtime registration event");
    auto* raw=host.get();
    const auto thread=CreateThread(nullptr,0,Worker,raw,0,nullptr);
    if(!thread) {CloseHandle(host->wake);throw std::runtime_error("cannot create metadata worker");}
    Instance()=host.release();
    CloseHandle(thread);return record;
}
}
RuntimeSubscription RegisterRuntimeClient(RuntimeVersion version,std::shared_ptr<RuntimeClient> client) {
    return RuntimeSubscription(Register(std::move(client),version));
}
RuntimeSubscription RegisterRuntimeClient(std::shared_ptr<RuntimeClient> client) {
    return RegisterRuntimeClient(detail::CurrentRuntimeProfile().version,std::move(client));
}
struct RuntimeSubscription::State {std::shared_ptr<detail::RuntimeClientRecord> record;};
RuntimeSubscription::RuntimeSubscription(std::shared_ptr<detail::RuntimeClientRecord> record) {
    try {state_=new State{record};}catch(...){record->stop.store(true);throw;}
}
RuntimeSubscription::~RuntimeSubscription() {RequestStop();delete state_;}
RuntimeSubscription::RuntimeSubscription(RuntimeSubscription&& other) noexcept:state_(other.state_) {other.state_=nullptr;}
RuntimeSubscription& RuntimeSubscription::operator=(RuntimeSubscription&& other) noexcept {
    if(this!=&other){RequestStop();delete state_;state_=other.state_;other.state_=nullptr;}return *this;
}
void RuntimeSubscription::RequestStop() noexcept {if(state_)state_->record->stop.store(true);}
RuntimeClientStatus RuntimeSubscription::Status() const {
    if(state_)return state_->record->Read();
    RuntimeClientStatus status;status.phase=RuntimeHostPhase::Stopped;return status;
}
RuntimeHostStatus ReadRuntimeHostStatus() {
    std::lock_guard<std::mutex> lock(StartMutex());
    if(!Instance()) {RuntimeHostStatus out;out.phase=RuntimeHostPhase::Stopped;return out;}
    std::lock_guard<std::mutex> state(Instance()->status_mutex);return Instance()->status;
}
namespace detail {
std::shared_ptr<RuntimeMetadata> RetainRuntimeMetadata(RuntimeMetadata& provider) {
    std::lock_guard<std::mutex> lock(StartMutex());
    if(!Instance() || Instance()->metadata.get()!=&provider)
        throw std::logic_error("resident presentation service needs the actual RuntimeHost provider");
    return Instance()->metadata;
}
}
}
