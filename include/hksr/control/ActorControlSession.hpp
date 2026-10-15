#pragma once
#include "../world/WorldPlacement.hpp"
#include "../entity/EntityLifecycle.hpp"
#include <optional>
#include <memory>
#include <string>
#include <cstdint>

namespace hksr::sdk_v1 {
class SceneObjects;
class VisualObjects;
struct ActorControlBindings;
enum class GazeMode { Automatic, Disabled, Entity, Point };
struct GazeRequest { GazeMode mode=GazeMode::Automatic; Position3 point;float max_distance=0; }; // 0: no extra distance limit
enum class PlacementPhase { Idle, Waiting, Applied, Failed };
enum class GazePhase { Automatic, Following, Disabled, TargetUnavailable, Failed, OutOfRange, Restoring };
enum class ControlPhase { Waiting, Running, Returned, Failed, Cancelled, Superseded };
struct ControlResult {
    std::uint64_t id=0;
    ControlPhase phase=ControlPhase::Waiting;
    std::string detail;
    bool Terminal() const noexcept {return phase!=ControlPhase::Waiting && phase!=ControlPhase::Running;}
};
// Live receipt, read only on the official owner thread. Copy the value before
// publishing to other threads. Terminal results never change; retaining a
// receipt does not retain an entity or any engine resource.
using ControlReceipt=std::shared_ptr<const ControlResult>;
struct ActorControlStatus {
    PlayerPlacement live;
    bool placement_pending=false;
    PlacementPhase placement=PlacementPhase::Idle;
    GazePhase gaze=GazePhase::Automatic;
    std::string placement_error,gaze_error;
};
struct ActorControlServices {
    ManagedObjects& objects;
    const SceneObjects& scene;
    const VisualObjects& visuals;
};
// One entity incarnation. Owns component roots, the private gaze point, original
// automatic-target policy and post-movement placement receipts. No logical
// instance IDs, player selection, UI strings, persistence or entity destruction.
// All methods belong to the official owner thread; Place/Look only queue values.
// Service's target is borrowed for that frame and checked against the owner world.
// Call Suspend before replacing the model, and Close before retiring the entity;
// keep calling Close until point destruction and reference release complete.
class HKSR_API ActorControlSession {
public:
    ActorControlSession(EntityToken,GameWorld*,const ActorControlBindings&);
    ~ActorControlSession(); // cancels native writes only; never calls the game
    ActorControlSession(const ActorControlSession&)=delete;
    ActorControlSession& operator=(const ActorControlSession&)=delete;
    // Optional ground policy is applied before the combined position setter.
    // False uses official direct navigation motion without terrain correction;
    // animation/navigation deltas still apply. True restores the session's
    // original movement mode and permits grounding, subject to official state
    // eligibility. Omission preserves policy; Close restores all original flags.
    ControlReceipt Place(Position3,Rotation4,bool relative,std::optional<bool> snap_to_ground={});
    ControlReceipt Look(GazeRequest);
    void Service(const WorldFrame&,ActorControlServices,GameEntity* target,bool presentation_ready);
    void Cancel();
    // Waiting means the official fade is still advancing, not a failed call.
    // Keep servicing with presentation_ready=false and poll before replacing
    // the model. Returned permits handoff; Failed/Cancelled do not.
    ControlPhase Suspend();
    bool Close(const WorldFrame&);
    const ActorControlStatus& Status() const;
    PlayerPlacement PlannedPlacement(PlayerPlacement fallback={}) const;
private:
    struct State; State* state_;
};
}
