#pragma once
#include "../../metadata/RuntimeMetadata.hpp"
#include "../../managed/ManagedRoots.hpp"
#include <functional>

namespace hksr::sdk_v1 {
// Completion arguments are borrowed for this call only. The second argument
// is the unboxed, one-reference TriggerEffectCallbackParams VALUE, not a box.
// Completion may happen inside submission, before official initialization ends.
struct EffectCompletionEvent {void* effect=nullptr;void* parameters=nullptr;};
struct EffectCompletionLayout {
    std::size_t method_ptr=0,invoke_impl=0,method_code=0;
};
struct EffectCompletionAllocation {
    std::function<ObjectResult()> allocate;
    EffectCompletionLayout layout;
};
// Narrow bridge for the qualified effect-completion delegate's direct Invoke.
// Not a general CLR delegate constructor: reflection, Combine/BeginInvoke and
// arbitrary signatures are unsupported. The bridge calls no game API; the
// sink should only record the result for later servicing, not mutate the game.
// Create/Close run on the owning engine thread; callbacks may be reentrant.
class HKSR_API EffectCompletion {
public:
    EffectCompletion(EffectCompletionAllocation allocation,ReferenceMethods references,
        std::function<void(EffectCompletionEvent)> sink);
    ~EffectCompletion(); // disconnects native dispatch only; no engine calls
    EffectCompletion(const EffectCompletion&)=delete;
    EffectCompletion& operator=(const EffectCompletion&)=delete;
    ObjectResult Create();
    // Disconnect first; already retained managed delegates become inert. This
    // releases only our GC root, never the effect or its pooled parameters.
    // A sink already copied by a concurrent invocation may finish: captures
    // must retain their own native state, not borrow the owner's this pointer.
    bool Close();
private:
    struct State;State* state_;
};
// Resolve current constructed Action<MonoEffect,TriggerEffectCallbackParams>
// from the official setter parameter, not open Action`2 or an offline RVA.
HKSR_API EffectCompletionAllocation ResolveEffectCompletion460(RuntimeMetadata& metadata);
HKSR_API EffectCompletionLayout ResolveEffectCompletionLayout460(const std::vector<DeclaredField>& fields);
}
