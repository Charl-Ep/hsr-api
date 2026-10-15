#pragma once
#include <hksr/presentation/events/AnimationEvents.hpp>
#include <hksr/managed/ManagedArrays.hpp>
#include <hksr/managed/ManagedLists.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/unity/SceneObjects.hpp>
#include <hksr/presentation/timeline/EventRecordLifetime.hpp>
#include <hksr/entity/LifecycleHooks.hpp>

namespace hksr::sdk_v1 {
struct EventRegistrationReceipt {
    ObjectResult registration;
    CallResult detach,dispose;
    bool register_attempted=false,detach_attempted=false,dispose_attempted=false;
    bool present=false,observed=false;
    bool retained=false;
    ManagedRootId registration_root=0;
    EventRecordToken lifetime;
    EventRecordEnd official_end;
    std::string error;
};
struct EventFileReceipt : EventRegistrationReceipt {
    void* path=nullptr;
    std::vector<EventRegistrationReceipt> previous;
    void* inline_groups=nullptr;
};
struct EventInputStatus {
    bool prepared=false,attached=false,closing=false,owner_ended=false;
    bool identity_changed=false,roots_closed=false;
    // GC closure is deliberately separate from successful task retirement.
    bool retirement_complete=false;
    std::vector<std::string> errors;
};

struct EventRegistrationResult {
    bool complete = false;
    std::vector<std::string> errors;
};
// One event-consumer incarnation on one owned entity. Engine-thread only.
// Only supplemental registrations are retired; baseline registrations are not
// owned. Retire while live, or notify OwnerEnded/Superseded as appropriate,
// then Close the GC roots. Destruction never invokes the engine.
class HKSR_API EventRegistrationLease {
public:
    EventRegistrationLease(ReferenceMethods, AnimationEvents, ManagedReferenceList);
    ~EventRegistrationLease();
    EventRegistrationLease(const EventRegistrationLease&) = delete;
    EventRegistrationLease& operator=(const EventRegistrationLease&) = delete;
    void Prepare(const ManagedArrays&, const SceneObjects&, ObjectResult baseline, ObjectResult desired);
    void PrepareInline(void* rooted_groups);
    void Attach(void* owned_entity, void* event_component, EntityToken owner_token = {});
    void Service();
    EventRegistrationResult Retire();
    void OwnerEnded();
    void Superseded();
    EventRegistrationResult Close();
    const EventInputStatus& Status() const;
    const std::vector<EventFileReceipt>& Files() const;
private:
    struct State;
    State* state_;
};
}
