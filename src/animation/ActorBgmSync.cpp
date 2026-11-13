#include <hksr/animation/ActorBgmSync.hpp>
#include <cmath>
#include <stdexcept>
#include <limits>

namespace hksr::api {
namespace {
void Text(const std::string& text, bool empty = false) {
    if ((!empty && text.empty()) || text.find('\0') != std::string::npos)
        throw std::invalid_argument("invalid BGM sync text");
}
void Phases(const std::vector<BgmSyncPhase>& phases) {
    if (phases.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        throw std::invalid_argument("BGM sync phase array too large");
    for (const auto& value : phases)
        if (!std::isfinite(value.start_time) || !std::isfinite(value.blend) || !std::isfinite(value.normalized_offset))
            throw std::invalid_argument("nonfinite BGM sync phase");
}
}
struct ActorBgmSync::State {
    ActorBgmSyncServices calls;
    bool closing = false, closed = false;
    explicit State(ActorBgmSyncServices value) : calls(std::move(value)) {
        if (!calls.start || !calls.observe || !calls.stop || !calls.current || !calls.close)
            throw std::invalid_argument("BGM sync services incomplete");
    }
};
ActorBgmSync::ActorBgmSync(ActorBgmSyncServices value) : state_(std::make_unique<State>(std::move(value))) {}
ActorBgmSync::~ActorBgmSync() = default;
BgmSyncStartResult ActorBgmSync::Start(const BgmSyncConfig& config) {
    auto& s = *state_;
    Text(config.state);
    if (!std::isfinite(config.bpm) || config.bpm <= 0 || config.chapters.empty())
        throw std::invalid_argument("invalid BGM sync configuration");
    if (config.chapters.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        throw std::invalid_argument("BGM sync chapter array too large");
    for (const auto& chapter : config.chapters) {
        Text(chapter.cue, true);
        Phases(chapter.phases);
        if (chapter.double_speed_phases)
            Phases(*chapter.double_speed_phases);
    }
    if (s.closing || !s.calls.current())
        return {BgmSyncStartState::Failed, {}, "BGM sync actor/model/controller ended"};
    return s.calls.start(config);
}
BgmSyncObservation ActorBgmSync::Observe(BgmSyncToken token) {
    return state_->calls.observe(token);
}
bool ActorBgmSync::Stop(BgmSyncToken token) {
    return !token.value || state_->calls.stop(token);
}
bool ActorBgmSync::Current() const {
    return !state_->closing && state_->calls.current();
}
bool ActorBgmSync::Close() {
    auto& s = *state_;
    if (s.closed)
        return true;
    s.closing = true;
    s.closed = s.calls.close();
    return s.closed;
}
}
