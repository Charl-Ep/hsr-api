#pragma once
#include <hksr/metadata/MethodResolver.hpp>
#include <functional>
#include <memory>
#include <optional>

namespace hksr::sdk_v1 {
enum class ActorInputKind { OutfitIdentity, WeaponVisibility };
// Qualified instance methods with one managed-reference argument. Replaces
// that argument only; the original method always runs and owns its behavior.
class ActorInputPolicy {
public:
    using Handler=std::function<std::optional<void*>(void*,void*)>;
    HKSR_API ActorInputPolicy(ActorInputKind,Method,Handler);
    HKSR_API ~ActorInputPolicy();
    HKSR_API void ActivateOnCurrentThread();
    HKSR_API void Stop() noexcept;
    ActorInputPolicy(const ActorInputPolicy&)=delete;
    ActorInputPolicy& operator=(const ActorInputPolicy&)=delete;
private:
    struct State;std::shared_ptr<State> state_;
};
}
