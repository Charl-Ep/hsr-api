#pragma once
#include "../../Export.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace hksr::api {
using AttachmentScaleValue = std::array<float, 3>;
struct AttachmentScaleServices {
    std::function<bool()> current;
    std::function<void(const std::string&, AttachmentScaleValue, bool withdrawing, std::uint64_t revision)> apply;
    std::function<void(const std::string&)> restore;
    std::function<bool()> close;
};
// Persistent official AttachPointSet contributions, withdrawn on the owner frame.
// Actor input roots are acquired at the first Set; a failed Set remains owned and
// must be closed. Close during an operation returns false and drains afterwards.
class HKSR_API AttachmentScale {
  public:
    explicit AttachmentScale(AttachmentScaleServices);
    ~AttachmentScale(); // no engine calls
    AttachmentScale(const AttachmentScale&) = delete;
    AttachmentScale& operator=(const AttachmentScale&) = delete;
    void Set(std::uint64_t scope, const std::string& point, AttachmentScaleValue);
    bool Release(std::uint64_t scope);
    bool Close();
    const std::string& Error() const;

  private:
    struct State;
    State* state_;
};
} // namespace hksr::api
