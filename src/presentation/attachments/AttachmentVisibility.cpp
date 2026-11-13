#include <hksr/presentation/attachments/AttachmentVisibility.hpp>
#include <algorithm>
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>

namespace hksr::api {
struct AttachmentVisibility::State {
    struct Contribution {
        std::uint64_t scope;
        bool value;
    };
    struct Point {
        bool baseline;
        std::optional<bool> written;
        std::vector<Contribution> values;
    };
    AttachmentVisibilityServices services;
    std::map<std::string, Point> points;
    std::string error;
    bool closing = false, closed = false;
    unsigned operations = 0;
    struct Operation {
        State& state;
        explicit Operation(State& value) : state(value) {
            ++state.operations;
        }
        ~Operation() {
            --state.operations;
        }
    };
    explicit State(AttachmentVisibilityServices input) : services(std::move(input)) {
        if (!services.current || !services.read || !services.write || !services.close)
            throw std::invalid_argument("attachment visibility services incomplete");
    }
    bool Release(std::optional<std::uint64_t> scope) {
        if (closed)
            return true;
        try {
            if (!services.current())
                points.clear();
            for (auto it = points.begin(); it != points.end();) {
                auto& point = it->second;
                auto next = point.values;
                next.erase(
                    std::remove_if(next.begin(), next.end(), [&](auto v) { return !scope || v.scope == *scope; }),
                    next.end());
                if (next.size() != point.values.size()) {
                    const auto actual = services.read(it->first);
                    if (point.written && actual == *point.written) {
                        const auto desired = next.empty() ? point.baseline : next.back().value;
                        if (actual != desired)
                            services.write(it->first, desired, 0);
                        point.written = desired;
                    } else {
                        // An official animation/event write takes precedence.
                        point.baseline = actual;
                        point.written.reset();
                    }
                    point.values = std::move(next);
                }
                if (point.values.empty())
                    it = points.erase(it);
                else
                    ++it;
            }
            error.clear();
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
};
AttachmentVisibility::AttachmentVisibility(AttachmentVisibilityServices services)
    : state_(new State(std::move(services))) {}
AttachmentVisibility::~AttachmentVisibility() {
    delete state_;
}
const std::string& AttachmentVisibility::Error() const {
    return state_->error;
}
void AttachmentVisibility::Set(std::uint64_t scope, const std::string& name, bool visible, int delay) {
    auto& s = *state_;
    if (!scope || name.empty() || delay < 0)
        throw std::invalid_argument("invalid attachment visibility input");
    if (s.operations)
        throw std::logic_error("attachment visibility operation already active");
    State::Operation operation(s);
    if (s.closing || !s.services.current())
        throw std::runtime_error("attachment model ended or replaced");
    if (s.closing)
        throw std::runtime_error("attachment visibility closing");
    auto found = s.points.find(name);
    if (found == s.points.end()) {
        const auto baseline = s.services.read(name);
        if (s.closing)
            throw std::runtime_error("attachment visibility closing");
        found = s.points.emplace(name, State::Point{baseline, {}, {}}).first;
    }
    auto& point = found->second;
    const auto actual = s.services.read(name);
    if (s.closing)
        throw std::runtime_error("attachment visibility closing");
    if (point.written && actual != *point.written) {
        point.baseline = actual;
        point.written.reset();
    }
    point.values.erase(
        std::remove_if(point.values.begin(), point.values.end(), [&](auto v) { return v.scope == scope; }),
        point.values.end());
    point.values.push_back({scope, visible});
    // Keep the receipt before a call whose failure may follow a native write.
    point.written = visible;
    s.services.write(name, visible, delay);
}
bool AttachmentVisibility::Release(std::uint64_t scope) {
    if (state_->operations)
        return false;
    State::Operation operation(*state_);
    return state_->Release(scope);
}
bool AttachmentVisibility::Close() {
    auto& s = *state_;
    if (s.closed)
        return true;
    s.closing = true;
    if (s.operations)
        return false;
    State::Operation operation(s);
    if (!s.Release({}))
        return false;
    try {
        s.closed = s.services.close();
        if (!s.closed)
            s.error = "attachment reference release incomplete";
    } catch (const std::exception& e) {
        s.error = e.what();
    }
    return s.closed;
}
} // namespace hksr::api
