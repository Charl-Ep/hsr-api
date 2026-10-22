#pragma once
#include "SoundAccess.hpp"
#include "SoundCallbacks.hpp"
#include <memory>
#include <string>

namespace hksr::sdk_v1 {
struct SoundRequestServices {
    ReferenceMethods references;
    SoundAccess access;
    SoundCallbackAllocation callbacks;
    // Optional engine-thread owner/model/world check. The factory binds this
    // to the original entity generation and Art, never a replacement model.
    std::function<bool()> current;
};
struct SoundRequestStatus {
    SoundPostResult submission;
    CallResult stop;
    bool attempted=false,accepted=false,started=false,ended=false,cancelled=false,closed=false;
    std::uint64_t foreign_callbacks=0;
    std::string error;
};
// One audio request, not an actor controller. Start and Service run on the
// owning engine thread. Caller supplies retained inputs from its owned model
// and cancels before that model/world retires. Service(false) also cancels.
// Closed means roots/dispatch released; ended means an actual end callback.
// Stop submission does not await a callback (queued events may send none).
// If the engine silently drops a bank-waiting entry it remains awaiting start
// until its caller cancels; there is no fabricated completion or timeout.
// Engine logical IDs are not globally unique execution identities. Stop uses
// the engine's ID-group semantics; callers must not alias them as task IDs.
class HKSR_API OwnedSoundRequest {
public:
    explicit OwnedSoundRequest(SoundRequestServices);
    ~OwnedSoundRequest(); // disconnects only, no engine calls
    OwnedSoundRequest(const OwnedSoundRequest&)=delete;
    OwnedSoundRequest& operator=(const OwnedSoundRequest&)=delete;
    void Start(void* manager,void* event_name,void* emitter);
    void Cancel(); // owner-thread, data-only
    void Service(bool owner_current=true);
    const SoundRequestStatus& Status() const;
private:
    struct State;State* state_;
};
}
