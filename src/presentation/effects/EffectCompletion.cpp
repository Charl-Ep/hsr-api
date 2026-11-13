#include <hksr/presentation/effects/EffectCompletion.hpp>
#include "../../../adapters/presentation/CallbackBridges.hpp"
#include <hksr/native/MetadataReader.hpp>
#include <map>
#include <mutex>
#include <stdexcept>
#include <limits>

namespace hksr::api {
namespace {
using Sink = std::function<void(EffectCompletionEvent)>;
struct Dispatch {
    std::mutex mutex;
    std::uintptr_t next = 0;
    std::map<std::uintptr_t, Sink> sinks;
    static Dispatch& Get() {
        static auto* state = new Dispatch;
        return *state;
    }
    std::uintptr_t Add(Sink sink) {
        std::lock_guard<std::mutex> guard(mutex);
        if (next == std::numeric_limits<std::uintptr_t>::max())
            throw std::overflow_error("effect callback identity exhausted");
        const auto id = ++next;
        sinks.emplace(id, std::move(sink));
        return id;
    }
    void Remove(std::uintptr_t id) {
        std::lock_guard<std::mutex> guard(mutex);
        sinks.erase(id);
    }
};
void Complete(void* cookie, EffectCompletionEvent event) noexcept {
    // Dispatch on the native callback stack without extending its borrowed
    // arguments. Copy the sink before allowing reentrancy or disconnection.
    try {
        Sink sink;
        {
            auto& dispatch = Dispatch::Get();
            std::lock_guard<std::mutex> guard(dispatch.mutex);
            const auto found = dispatch.sinks.find(reinterpret_cast<std::uintptr_t>(cookie));
            if (found != dispatch.sinks.end())
                sink = found->second;
        }
        if (sink)
            sink(event);
    } catch (...) {
    } // Do not unwind through the official loader.
}
}
struct EffectCompletion::State {
    EffectCompletionAllocation allocation;
    ManagedRoots roots;
    Sink sink;
    ManagedRootId root = 0;
    std::uintptr_t cookie = 0;
    void* object = nullptr;
    bool attempted = false, closed = false;
    State(EffectCompletionAllocation a, ReferenceMethods r, Sink s)
        : allocation(std::move(a)), roots(r), sink(std::move(s)) {}
    void Disconnect() {
        if (cookie) {
            Dispatch::Get().Remove(cookie);
            cookie = 0;
        }
    }
};
EffectCompletion::EffectCompletion(EffectCompletionAllocation a, ReferenceMethods r, Sink sink)
    : state_(new State(std::move(a), r, std::move(sink))) {}
EffectCompletion::~EffectCompletion() {
    state_->Disconnect();
    delete state_;
}
ObjectResult EffectCompletion::Create() {
    auto& s = *state_;
    if (s.closed)
        return {};
    if (s.attempted)
        return s.object ? ObjectResult{{true, 0}, s.object} : ObjectResult{};
    s.attempted = true;
    if (!s.allocation.allocate || !s.sink)
        return {};
    const auto created = s.allocation.allocate();
    if (!created.call.returned || !created.object)
        return created;
    s.root = s.roots.Keep(created.object);
    if (!s.roots.Status(s.root).ready)
        return {};
    s.cookie = Dispatch::Get().Add(s.sink);
    const auto& fields = s.allocation.layout;
    const auto entry = reinterpret_cast<void*>(&detail::EffectCompletionBridge<&Complete>);
    // Fresh managed allocation is zero-initialized. Only unmanaged IntPtr
    // fields are written; target/method metadata remain null. Qualified direct
    // Invoke reads invoke_impl, method_code and method; Complete ignores the
    // latter. Never pass a native function address to the managed .ctor, which
    // interprets its IntPtr as MethodInfo in this game.
    if (!detail::BindCallbackDelegate(created.object, fields, entry, s.cookie)) {
        s.Disconnect();
        return {};
    }
    s.object = created.object;
    return created;
}
bool EffectCompletion::Close() {
    auto& s = *state_;
    s.closed = true;
    s.Disconnect();
    s.object = nullptr;
    if (!s.root)
        return true;
    if (!s.roots.Release(s.root).complete)
        return false;
    s.roots.Forget(s.root);
    s.root = 0;
    return true;
}
}
