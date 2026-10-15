#pragma once
#include "../metadata/RuntimeMetadata.hpp"
#include "RuntimeThread.hpp"
#include "../world/WorldFrames.hpp"
#include <memory>

namespace hksr::sdk_v1 {
struct RuntimeCapabilities {
    bool entity_lifecycle=false,model_release=false,event_record_lifetime=false;
};
enum class RuntimeHostPhase { Starting, Running, Stopping, Stopped, Failed };
struct RuntimeHostStatus {
    RuntimeHostPhase phase=RuntimeHostPhase::Starting;
    RuntimeCapabilities capabilities;
    RuntimeThreadStatus attachment;
    bool startup_finished=false,worker_finished=false,world_hook=false,resident=false;
    unsigned long worker_thread=0,consumer_thread=0;
    std::uint64_t frames=0,skipped_frames=0,frame_failures=0;
    std::vector<std::string> startup_errors;
    std::string last_frame_error;
};
class HKSR_API RuntimeClient {
public:
    virtual ~RuntimeClient()=default;
    // Attached metadata worker, NOT Unity's world thread. Bind declarations
    // and construct native consumers only; do not create engine objects here.
    virtual void Prepare(RuntimeMetadata&,RuntimeCapabilities)=0;
    // Optional observation scope around the batch: all BeforeFrame calls,
    // then all Frame calls, then AfterFrame in reverse order. A client's
    // failures are isolated; AfterFrame still runs after a throwing callback.
    // These are official-thread callbacks, not worker/UI notifications.
    virtual void BeforeFrame(const WorldFrame&,bool) {}
    // Actual official update thread. true means ALL client callback owners
    // have drained and the host may release this client. No wait in DllMain.
    virtual bool Frame(const WorldFrame&,bool stop_requested)=0;
    virtual void AfterFrame(const WorldFrame&,bool) {}
};
struct RuntimeClientStatus {
    RuntimeHostPhase phase=RuntimeHostPhase::Starting;
    bool prepared=false,stop_requested=false;
    std::uint64_t frames=0,frame_failures=0;
    std::string preparation_error,last_frame_error;
};
namespace detail { struct RuntimeClientRecord; }
class RuntimeSubscription;
HKSR_API RuntimeSubscription RegisterRuntimeClient(RuntimeVersion version,std::shared_ptr<RuntimeClient> client);
HKSR_API RuntimeSubscription RegisterRuntimeClient(std::shared_ptr<RuntimeClient> client);
// One client's ownership, not ownership of the process host. Moving transfers
// the stop obligation; destruction requests stop but never waits or calls game
// code. Keep the handle to observe completion on later official frames.
class HKSR_API RuntimeSubscription {
public:
    RuntimeSubscription()=default;
    ~RuntimeSubscription();
    RuntimeSubscription(RuntimeSubscription&&) noexcept;
    RuntimeSubscription& operator=(RuntimeSubscription&&) noexcept;
    RuntimeSubscription(const RuntimeSubscription&)=delete;
    RuntimeSubscription& operator=(const RuntimeSubscription&)=delete;
    void RequestStop() noexcept;
    RuntimeClientStatus Status() const;
private:
    explicit RuntimeSubscription(std::shared_ptr<detail::RuntimeClientRecord>);
    struct State;
    State* state_=nullptr;
    friend HKSR_API RuntimeSubscription RegisterRuntimeClient(RuntimeVersion,std::shared_ptr<RuntimeClient>);
    friend HKSR_API RuntimeSubscription RegisterRuntimeClient(std::shared_ptr<RuntimeClient>);
};
// Register outside DllMain. All clients share one pinned host/world hook.
// Prepare runs on an attached worker, serialized against all client frames;
// official updates themselves never wait on preparation. Registration does
// not imply preparation success. A failed host is not automatically restarted.
// Stopping one subscription cannot stop another. Keep client code loaded until
// Stopped/Failed AND separately registered callback owners are gone; this is
// not a hot-unload API. Empty hosts remain available for later registration.
HKSR_API RuntimeHostStatus ReadRuntimeHostStatus();
}
