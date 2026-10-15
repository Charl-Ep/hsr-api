#pragma once
#include "EventRegistrationLease.hpp"
#include "../../entity/NpcCreationSession.hpp"

namespace hksr::sdk_v1 {
struct NpcEventBindings;
struct NpcEventSupplementStatus {
    bool prepared=false,enabled=true,closing=false,closed=false;
    std::uint64_t incarnations=0,incomplete_retirements=0;
    std::size_t retiring=0;
    ObjectResult data_component,event_component,baseline_config;
    std::vector<std::string> errors;
};
// Registers only desired event files absent from the owned NPC's baseline.
// Service before NPC retirement, on its official frame. The NPC session must
// outlive this supplement. Disabling retires registrations but keeps sources
// for reuse; RequestClose also drains sources and is terminal.
class HKSR_API NpcEventSupplement {
public:
    NpcEventSupplement(NpcCreationSession&,GameWorld*,ReferenceMethods,NpcEventBindings);
    ~NpcEventSupplement(); // native-only; keep servicing until Status().closed
    NpcEventSupplement(const NpcEventSupplement&)=delete;
    NpcEventSupplement& operator=(const NpcEventSupplement&)=delete;
    void Prepare(AnimationInputTypes,ObjectResult desired_paths);
    void SetEnabled(bool);
    void RequestClose();
    void Service(const WorldFrame&);
    bool Quiescent() const;
    // No live registration remains; GC cleanup can still be pending.
    bool InputsDetached() const;
    const NpcEventSupplementStatus& Status() const;
    const EventRegistrationLease* Active() const;
private:
    struct State;
    State* state_;
};
}
