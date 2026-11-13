#pragma once
#include <hksr/metadata/MethodResolver.hpp>
#include <hksr/managed/References.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace hksr::api::detail {
struct AudioPostArguments {
    void *event_name = nullptr, *emitter = nullptr, *callback = nullptr, *cookie = nullptr, *metadata = nullptr;
    std::uint32_t flags = 0;
};
struct AudioPostOperation {
    CallResult call;
    bool accepted = false;
    std::uint32_t native_code = 0;
};
struct AudioPostOpen {
    CallResult call;
    std::uint32_t id = 0;
};
struct AudioPostOperations {
    Method entry;
    ReferenceMethods references;
    std::function<AudioPostOpen(const AudioPostArguments&)> open;
    std::function<ObjectResult(std::uint32_t)> lock;
    std::function<AudioPostOperation(void*, std::uint32_t)> enqueue;
    std::function<AudioPostOperation(std::uint32_t)> unlock, play, stop, close;
};
// One process hook, independent owner-thread subscriptions. Selection performs
// only client-owned identity checks; native posting occurs outside router locks.
class AudioPostRoute {
  public:
    using Select = std::function<std::optional<std::uint32_t>(const AudioPostArguments&)>;
    using Trace = std::function<void(const std::string&)>;
    HKSR_API AudioPostRoute(AudioPostOperations, Select, Trace = {});
    HKSR_API ~AudioPostRoute();
    AudioPostRoute(const AudioPostRoute&) = delete;
    AudioPostRoute& operator=(const AudioPostRoute&) = delete;
    HKSR_API void Stop() noexcept;
    HKSR_API bool Collect();

  private:
    struct State;
    std::shared_ptr<State> state_;
};
}
