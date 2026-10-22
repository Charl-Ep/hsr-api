#pragma once
#include "../../metadata/RuntimeMetadata.hpp"
#include "../../managed/ManagedRoots.hpp"
#include <functional>
#include <memory>

namespace hksr::sdk_v1 {
struct SoundDelegateLayout {std::size_t method_ptr=0,invoke_impl=0,method_code=0;};
struct SoundDelegateAllocation {
    std::function<ObjectResult()> allocate;
    SoundDelegateLayout layout;
};
struct SoundCallbackAllocation {SoundDelegateAllocation began,ended;};
enum class SoundCallbackKind { Began, Ended };
// No borrowed AkCallbackInfo is retained or inspected after the callback.
struct SoundCallbackEvent {
    SoundCallbackKind kind=SoundCallbackKind::Ended;
    std::uint32_t logical_id=0;
    bool has_info=false;
};
struct SoundCallbackObjects {ObjectResult began,ended;};
// Direct Invoke bridges for the two exact AudioManager delegate signatures.
// Callbacks record values only; they never call engine APIs. Close disconnects
// first, including managed holders which outlive these GC roots. A concurrent
// sink already copied by dispatch may finish and must own its native captures.
class HKSR_API SoundCallbacks {
public:
    SoundCallbacks(SoundCallbackAllocation,ReferenceMethods,std::function<void(SoundCallbackEvent)>);
    ~SoundCallbacks(); // disconnect only; explicit Close on the engine thread
    SoundCallbacks(const SoundCallbacks&)=delete;
    SoundCallbacks& operator=(const SoundCallbacks&)=delete;
    SoundCallbackObjects Create();
    bool Close();
private:
    struct State;State* state_;
};
HKSR_API SoundDelegateLayout ResolveSoundDelegateLayout460(const std::vector<DeclaredField>&);
HKSR_API SoundCallbackAllocation ResolveSoundCallbacks460(RuntimeMetadata&);
}
