#pragma once
#include "../Export.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace hksr::api {
struct BgmSyncPhase {
    float start_time = 0, blend = 0, normalized_offset = 0;
};
struct BgmSyncChapter {
    std::string cue;
    std::vector<BgmSyncPhase> phases;
    std::optional<std::vector<BgmSyncPhase>> double_speed_phases;
};
struct BgmSyncConfig {
    std::string state;
    float bpm = 100;
    std::vector<BgmSyncChapter> chapters;
};
struct BgmSyncToken {
    std::uint64_t value = 0;
};
enum class BgmSyncStartState { Accepted, Pending, Unsupported, Failed };
struct BgmSyncStartResult {
    BgmSyncStartState state = BgmSyncStartState::Failed;
    BgmSyncToken token;
    std::string detail;
};
struct BgmSyncObservation {
    bool accepted = false, active = false, drained = false;
    std::string error;
};
struct ActorBgmSyncServices {
    std::function<BgmSyncStartResult(const BgmSyncConfig&)> start;
    std::function<BgmSyncObservation(BgmSyncToken)> observe;
    std::function<bool(BgmSyncToken)> stop;
    std::function<bool()> current, close;
};
// Synchronizes one owned model with the shared official BGM. It neither plays
// music nor starts body animation. Start after the intended body has started.
class ActorBgmSync {
  public:
    HKSR_API explicit ActorBgmSync(ActorBgmSyncServices);
    HKSR_API ~ActorBgmSync(); // explicit Close; no native calls
    HKSR_API BgmSyncStartResult Start(const BgmSyncConfig&);
    HKSR_API BgmSyncObservation Observe(BgmSyncToken);
    HKSR_API bool Stop(BgmSyncToken);
    HKSR_API bool Current() const;
    HKSR_API bool Close();

  private:
    struct State;
    std::unique_ptr<State> state_;
};
}
