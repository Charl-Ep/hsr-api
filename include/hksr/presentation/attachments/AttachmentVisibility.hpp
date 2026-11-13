#pragma once
#include "../../Export.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace hksr::api {
struct AttachmentVisibilityServices {
    std::function<bool()> current;
    std::function<bool(const std::string&)> read;
    std::function<void(const std::string&, bool, int)> write;
    std::function<bool()> close;
};
// Owner-frame overrides on one initialized owned model. Scope withdrawal
// restores the next live contribution, or the captured native value. An
// independently changed native value is not overwritten during withdrawal.
// Actor input roots are acquired at the first Set. Close during an operation
// returns false; finish cleanup after that operation unwinds.
class HKSR_API AttachmentVisibility {
  public:
    explicit AttachmentVisibility(AttachmentVisibilityServices);
    ~AttachmentVisibility(); // no engine calls
    AttachmentVisibility(const AttachmentVisibility&) = delete;
    AttachmentVisibility& operator=(const AttachmentVisibility&) = delete;
    void Set(std::uint64_t scope, const std::string& point, bool visible, int delay = 0);
    bool Release(std::uint64_t scope);
    bool Close();
    const std::string& Error() const;

  private:
    struct State;
    State* state_;
};
} // namespace hksr::api
