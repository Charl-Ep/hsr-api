#pragma once
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/unity/OwnedUnityObjects.hpp>
#include <memory>
#include <vector>

namespace hksr::sdk_v1::detail {
// Birth receipts confined to one owned carrier's DPM wrapper factory. Normal
// Unity/game calls and constructors nested inside that wrapper ctor are ignored.
// Captures before the ctor, so a partial native failure is still recoverable.
class ProjectileWrapperCapture {
public:
    static void Prepare(MethodResolver&);
    ProjectileWrapperCapture(ReferenceMethods,UnityLifetimeMethods);
    ~ProjectileWrapperCapture();
    void Begin(void* carrier);
    void End() noexcept;
    const std::vector<void*>& Objects() const;
    bool Complete() const;
    bool Stop();
    bool Drain(bool destroy);
private:
    struct State;std::shared_ptr<State> state_;
};
}
