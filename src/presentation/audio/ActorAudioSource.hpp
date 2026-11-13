#pragma once
#include <hksr/managed/ManagedRoots.hpp>
#include <functional>
#include <stdexcept>
#include <vector>

namespace hksr::api::detail {
// A retained component is not evidence that it is still the entity's current
// component. The caller supplies its current identity before using this reader.
class ActorAudioSource {
  public:
    using Reader = std::function<void*()>;
    using Bind = std::function<Reader(void*)>;
    ActorAudioSource() = default;
    ActorAudioSource(const ActorAudioSource&) = delete;
    ActorAudioSource& operator=(const ActorAudioSource&) = delete;
    void Refresh(void* current, ManagedRoots& roots, std::vector<ManagedRootId>& retired, const Bind& bind,
                 const std::function<void()>& unindex) {
        if (current == component_)
            return;
        Reset(retired, unindex);
        if (!current)
            return;
        retired.reserve(retired.size() + 1);
        const auto key = roots.Keep(current);
        if (!roots.Status(key).ready) {
            retired.push_back(key);
            throw std::runtime_error("audio source retention failed");
        }
        try {
            auto reader = bind(current);
            if (!reader)
                throw std::runtime_error("audio source reader unavailable");
            component_ = current;
            root_ = key;
            reader_ = std::move(reader);
        } catch (...) {
            retired.push_back(key);
            throw;
        }
    }
    void Reset(std::vector<ManagedRootId>& retired, const std::function<void()>& unindex) {
        if (root_)
            retired.reserve(retired.size() + 1);
        unindex();
        reader_ = {};
        component_ = nullptr;
        if (root_) {
            retired.push_back(root_);
            root_ = 0;
        }
    }
    void* Emitter() const {
        return reader_ ? reader_() : nullptr;
    }

  private:
    void* component_ = nullptr;
    ManagedRootId root_ = 0;
    Reader reader_;
};
}
