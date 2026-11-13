#include <hksr/model/ModelScaleProgram.hpp>
#include <hksr/model/ActorModelPresentation.hpp>
#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <tuple>

namespace hksr::api {
namespace {
using Key = std::tuple<GameWorld*, GameEntity*, std::uint64_t>;
Key Identity(EntityOrigin origin) {
    return {origin.world, origin.token.entity, origin.token.generation};
}
void Validate(const ModelScaleInput& input) {
    if (!std::isfinite(input.base_scale) || !std::isfinite(input.outline_scale))
        throw std::invalid_argument("nonfinite model scale input");
    std::set<float> heights;
    for (const auto& row : input.multiplier)
        if (!std::isfinite(row.height) || !std::isfinite(row.factor) || !heights.insert(row.height).second)
            throw std::invalid_argument("invalid or duplicate model scale multiplier");
}
}
struct ModelScaleProgram::State {
    struct Contribution {
        std::uint64_t scope;
        ModelScaleSnapshot applied;
    };
    struct Entry {
        ModelScaleTarget target;
        std::optional<ModelScaleSnapshot> baseline, written;
        std::vector<Contribution> values;
        bool uncertain = false;
    };
    ModelScaleProgramServices services;
    std::map<Key, Entry> entries;
    std::vector<ModelScaleTarget> retired;
    std::string error;
    bool closing = false, busy = false;
    explicit State(ModelScaleProgramServices input) : services(std::move(input)) {
        if (!services.bind)
            throw std::invalid_argument("model scale binding service required");
    }
    bool Retire(Entry& entry) {
        if (!entry.target.close())
            return false;
        return true;
    }
    bool Release(std::optional<std::uint64_t> scope, const std::optional<Key>& participant = {}) {
        bool complete = true;
        for (auto it = entries.begin(); it != entries.end();) {
            if (participant && it->first != *participant) {
                ++it;
                continue;
            }
            auto& entry = it->second;
            try {
                if (!entry.target.current())
                    entry.values.clear();
                else {
                    auto next = entry.values;
                    next.erase(std::remove_if(
                                   next.begin(), next.end(),
                                   [&](const auto& value) { return participant || !scope || value.scope == *scope; }),
                               next.end());
                    if (next.size() != entry.values.size()) {
                        const auto actual = entry.target.read();
                        // A native failure may have prevented the immediate
                        // post-write observation. Retry that observation while
                        // retaining the original withdrawal responsibility.
                        if (entry.uncertain) {
                            entry.written = actual;
                            entry.uncertain = false;
                        }
                        auto desired = next.empty() ? *entry.baseline : next.back().applied;
                        // Keep independent native writers on either shared slot.
                        if (!entry.written || actual.factor != entry.written->factor)
                            desired.factor = actual.factor;
                        if (!entry.written || actual.shader_identity != entry.written->shader_identity ||
                            actual.outline_scale != entry.written->outline_scale)
                            desired.outline_scale = actual.outline_scale;
                        if (desired.factor != actual.factor || desired.outline_scale != actual.outline_scale) {
                            entry.target.restore(desired);
                            entry.written = entry.target.read();
                        } else
                            entry.written = actual;
                        entry.values = std::move(next);
                    }
                }
                if (entry.values.empty()) {
                    if (!Retire(entry)) {
                        complete = false;
                        ++it;
                    } else
                        it = entries.erase(it);
                } else
                    ++it;
            } catch (const std::exception& e) {
                error = e.what();
                complete = false;
                ++it;
            }
        }
        for (auto it = retired.begin(); it != retired.end();)
            try {
                if (it->close())
                    it = retired.erase(it);
                else {
                    complete = false;
                    ++it;
                }
            } catch (const std::exception& e) {
                error = e.what();
                complete = false;
                ++it;
            }
        if (complete)
            error.clear();
        return complete;
    }
};
ModelScaleProgram::ModelScaleProgram(ModelScaleProgramServices s) : state_(std::make_unique<State>(std::move(s))) {}
ModelScaleProgram::~ModelScaleProgram() = default;
const std::string& ModelScaleProgram::Error() const {
    return state_->error;
}
ModelScaleProgramResult ModelScaleProgram::Set(std::uint64_t scope, const std::vector<EntityOrigin>& targets,
                                               const ModelScaleInput& input) {
    auto& s = *state_;
    ModelScaleProgramResult out;
    if (!scope || s.closing || s.busy) {
        out.detail = "model scale program closed, reentered or unscoped";
        return out;
    }
    Validate(input);
    const auto value = input;
    s.busy = true;
    struct Exit {
        bool& flag;
        ~Exit() {
            flag = false;
        }
    } exit{s.busy};
    std::vector<State::Entry*> bound;
    std::set<Key> seen;
    try {
        for (const auto origin : targets) {
            if (!origin.world || !origin.token.entity || !origin.token.generation)
                throw ModelPresentationUnsupported("model scale requires explicit owned actor targets");
            const auto key = Identity(origin);
            if (!seen.insert(key).second)
                continue;
            auto existing = s.entries.find(key);
            if (existing != s.entries.end() && !existing->second.target.current()) {
                if (!existing->second.target.close())
                    s.retired.push_back(existing->second.target);
                s.entries.erase(existing);
                existing = s.entries.end();
            }
            if (existing == s.entries.end()) {
                const auto target = s.services.bind(origin);
                if (!target) {
                    out.state = ModelScaleProgramState::Pending;
                    out.detail = "waiting for owned scale target model";
                    return out;
                }
                if (Identity(target->origin) != key || !target->current || !target->read || !target->prepare ||
                    !target->multiplier || !target->apply || !target->restore || !target->close)
                    throw std::runtime_error("incomplete or foreign model scale binding");
                existing = s.entries.emplace(key, State::Entry{*target}).first;
            }
            auto& entry = existing->second;
            if (!entry.target.current()) {
                out.state = ModelScaleProgramState::Pending;
                out.detail = "scale target model changed during binding";
                return out;
            }
            const auto baseline = entry.target.read(); // preflight all targets before writes
            if (!entry.baseline)
                entry.baseline = baseline;
            else if (entry.written) {
                if (baseline.factor != entry.written->factor)
                    entry.baseline->factor = baseline.factor;
                if (baseline.shader_identity != entry.written->shader_identity ||
                    baseline.outline_scale != entry.written->outline_scale) {
                    entry.baseline->outline_scale = baseline.outline_scale;
                    entry.baseline->shader_identity = baseline.shader_identity;
                }
            }
            bound.push_back(&entry);
        }
        out.targets = bound.size();
        if (bound.empty())
            throw ModelPresentationUnsupported("model scale target collection is empty");
        for (auto* entry : bound)
            entry->target.prepare(value);
        float minimum = 1; // exact official initial value, not float max
        if (value.equal_ratio_by_min)
            for (const auto* entry : bound) {
                const auto ratio = entry->target.multiplier(value);
                if (!std::isfinite(ratio))
                    throw std::runtime_error("official model scale multiplier is nonfinite");
                minimum = std::min(minimum, ratio);
            }
        for (auto* entry : bound) {
            if (!entry->target.current())
                throw std::runtime_error("scale target model ended before apply");
            const auto prior_values = entry->values;
            const auto prior_written = entry->written;
            entry->values.erase(std::remove_if(entry->values.begin(), entry->values.end(),
                                               [&](const auto& prior) { return prior.scope == scope; }),
                                entry->values.end());
            entry->values.push_back({scope, entry->target.read()});
            entry->uncertain = true;
            try {
                entry->target.apply(value, minimum);
            } catch (const ModelPresentationUnsupported&) {
                entry->values = prior_values;
                entry->written = prior_written;
                entry->uncertain = false;
                throw;
            } catch (...) {
                try {
                    entry->written = entry->target.read();
                    entry->values.back().applied = *entry->written;
                    entry->uncertain = false;
                } catch (...) {
                }
                throw;
            }
            ++out.applied;
            entry->written = entry->target.read();
            entry->values.back().applied = *entry->written;
            entry->uncertain = false;
        }
        out.state = ModelScaleProgramState::Applied;
    } catch (const ModelPresentationUnsupported& e) {
        out.state = out.applied ? ModelScaleProgramState::Failed : ModelScaleProgramState::Unsupported;
        out.detail = e.what();
    } catch (const std::exception& e) {
        out.state = ModelScaleProgramState::Failed;
        out.detail = e.what();
    }
    return out;
}
bool ModelScaleProgram::Release(std::uint64_t scope) {
    if (state_->busy)
        return false;
    return state_->Release(scope);
}
bool ModelScaleProgram::ReleaseParticipant(EntityOrigin origin) {
    if (state_->busy)
        return false;
    return state_->Release({}, Identity(origin));
}
bool ModelScaleProgram::Close() {
    auto& s = *state_;
    s.closing = true;
    if (s.busy)
        return false;
    return s.Release({});
}
}
