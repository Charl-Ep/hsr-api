#include <hksr/presentation/attachments/AttachmentScale.hpp>
#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>

namespace hksr::api {
struct AttachmentScale::State {
    struct Contribution {
        std::uint64_t scope;
        AttachmentScaleValue value;
        std::uint64_t revision;
    };
    AttachmentScaleServices services;
    std::map<std::string, std::vector<Contribution>> points;
    std::string error;
    bool closing = false, closed = false;
    unsigned operations = 0;
    std::uint64_t revision = 0;
    struct Operation {
        State& state;
        explicit Operation(State& value) : state(value) {
            ++state.operations;
        }
        ~Operation() {
            --state.operations;
        }
    };
    explicit State(AttachmentScaleServices input) : services(std::move(input)) {
        if (!services.current || !services.apply || !services.restore || !services.close)
            throw std::invalid_argument("attachment scale services incomplete");
    }
    bool Release(std::optional<std::uint64_t> scope) {
        if (closed)
            return true;
        try {
            if (!services.current())
                points.clear();
            for (auto it = points.begin(); it != points.end();) {
                auto next = it->second;
                next.erase(
                    std::remove_if(next.begin(), next.end(), [&](auto v) { return !scope || v.scope == *scope; }),
                    next.end());
                if (next.size() != it->second.size()) {
                    if (next.empty())
                        services.restore(it->first);
                    else if (next.back().scope != it->second.back().scope)
                        services.apply(it->first, next.back().value, true, next.back().revision);
                    it->second = std::move(next);
                }
                if (it->second.empty())
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
AttachmentScale::AttachmentScale(AttachmentScaleServices services) : state_(new State(std::move(services))) {}
AttachmentScale::~AttachmentScale() {
    delete state_;
}
const std::string& AttachmentScale::Error() const {
    return state_->error;
}
void AttachmentScale::Set(std::uint64_t scope, const std::string& point, AttachmentScaleValue value) {
    auto& s = *state_;
    if (!scope || point.empty() || point.find('\0') != std::string::npos ||
        std::any_of(value.begin(), value.end(), [](float x) { return !std::isfinite(x) || x < 0; }))
        throw std::invalid_argument("invalid attachment scale input");
    if (s.operations)
        throw std::logic_error("attachment scale operation already active");
    State::Operation operation(s);
    if (s.closing || !s.services.current())
        throw std::runtime_error("attachment model ended or replaced");
    if (s.closing)
        throw std::runtime_error("attachment scale closing");
    auto& values = s.points[point];
    values.erase(std::remove_if(values.begin(), values.end(), [&](auto v) { return v.scope == scope; }), values.end());
    values.push_back({scope, value, ++s.revision}); // receipt precedes potentially partial native work
    s.services.apply(point, value, false, values.back().revision);
}
bool AttachmentScale::Release(std::uint64_t scope) {
    if (state_->operations)
        return false;
    State::Operation operation(*state_);
    return state_->Release(scope);
}
bool AttachmentScale::Close() {
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
            s.error = "attachment scale references pending";
    } catch (const std::exception& e) {
        s.error = e.what();
    }
    return s.closed;
}
} // namespace hksr::api
