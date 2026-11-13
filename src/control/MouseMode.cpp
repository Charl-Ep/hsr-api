#include <hksr/control/MouseMode.hpp>
#include "../../adapters/control/MouseModeThunk.hpp"
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include "../platform/NativeHooks.hpp"
#include <mutex>
#include <map>
#include <stdexcept>

namespace hksr::api {
namespace {
thread_local bool own_request = false;
struct OwnScope {
    bool prior = own_request;
    OwnScope() {
        own_request = true;
    }
    ~OwnScope() {
        own_request = prior;
    }
};
struct OwnerObservation {
    std::uint64_t serial = 0;
    bool forced = false;
};
struct Observer {
    detail::NativeHooks hooks;
    std::mutex mutex;
    std::map<void*, OwnerObservation> requests;
    std::size_t offset = 0;
    static Observer& Get() {
        static auto* value = new Observer;
        return *value;
    }
    void Record(void* object) noexcept {
        if (own_request || !object)
            return;
        bool forced = false;
        if (!native::CopyMemoryChecked(&forced, static_cast<const char*>(object) + offset, sizeof(forced)))
            return;
        try {
            std::lock_guard<std::mutex> guard(mutex);
            auto& row = requests[object];
            ++row.serial;
            row.forced = forced;
        } catch (...) {
        }
    }
    OwnerObservation Read(void* object) {
        std::lock_guard<std::mutex> guard(mutex);
        return requests[object];
    }
    static bool Before() {
        const bool own = own_request;
        own_request = false;
        return own;
    }
    static void* Original(int index) {
        return Get().hooks.Original(index);
    }
    static void After(void* object, bool own) {
        if (!own)
            Get().Record(object);
        own_request = own;
    }
};
void Returned(CallResult result) {
    if (!result.returned)
        throw std::runtime_error("official mouse mode call failed");
}
void OwnCall(Method method, void* object) {
    OwnScope own;
    Returned(native::CallVoid(method, object));
}
}
void ObserveMouseModeOwners(const MouseModeMethods& m) {
    auto& s = Observer::Get();
    s.offset = m.forced_offset;
    s.hooks.Install({{"Mouse.Force", m.force.code, detail::MouseModeThunk<Observer, 0>::Address()},
                     {"Mouse.SDKForce", m.force_sdk.code, detail::MouseModeThunk<Observer, 1>::Address()},
                     {"Mouse.Close", m.close.code, detail::MouseModeThunk<Observer, 2>::Address()}});
}
struct MouseModeLease::State {
    MouseModeMethods methods;
    References references;
    void* object = nullptr;
    int root = 0;
    bool external = false;
    std::uint64_t serial = 0;
    std::string status;
    State(MouseModeMethods m, ReferenceMethods r) : methods(m), references(r) {}
    bool Forced() const {
        bool value = false;
        if (!native::CopyMemoryChecked(&value, static_cast<const char*>(object) + methods.forced_offset, sizeof(value)))
            throw std::runtime_error("official cursor ownership read failed");
        return value;
    }
};
MouseModeLease::MouseModeLease(MouseModeMethods m, ReferenceMethods r) : state_(new State(m, r)) {}
MouseModeLease::~MouseModeLease() {
    delete state_;
} // Service(false) on official frame; no destructor engine calls
std::string MouseModeLease::Status() const {
    return state_->status;
}
void MouseModeLease::Service(bool requested) {
    auto& s = *state_;
    try {
        if (!s.object) {
            if (!requested)
                return;
            void* object = nullptr;
            Returned(native::Call(s.methods.instance, &object));
            if (!object)
                throw std::runtime_error("official mouse controller unavailable");
            const auto kept = s.references.Retain(object);
            if (!kept.call.returned || !kept.handle)
                throw std::runtime_error("mouse controller retention failed");
            s.object = object;
            s.root = kept.handle;
            s.external = s.Forced();
            s.serial = Observer::Get().Read(s.object).serial;
        }
        const auto current = Observer::Get().Read(s.object);
        if (current.serial != s.serial) {
            s.serial = current.serial;
            s.external = current.forced;
        }
        if (requested) {
            bool mode = false, visible = false;
            int lock = 0;
            Returned(native::Call(s.methods.use_mode, &mode, s.object));
            Returned(native::Call(s.methods.visible, &visible));
            Returned(native::Call(s.methods.lock, &lock));
            // AdventureInputProvider clears the force flag during its normal
            // update without leaving cursor mode. Reacquiring just for that
            // flag rebroadcasts the official cursor notification every frame.
            // Healthy mode is already the requested outcome; keep its inputs.
            if ((!mode || !visible || lock != 0) && !s.Forced()) {
                OwnCall(s.methods.force, s.object);
                Returned(native::Call(s.methods.use_mode, &mode, s.object));
                Returned(native::Call(s.methods.visible, &visible));
                Returned(native::Call(s.methods.lock, &lock));
            }
            // Focus recovery can invalidate native cursor state independently.
            if (s.Forced() && mode && (!visible || lock != 0))
                Returned(native::CallVoid(s.methods.refresh, s.object));
            s.status = mode ? "cursor mode active" : "cursor mode unavailable";
        } else {
            if (!s.external) {
                OwnCall(s.methods.close, s.object);
                Returned(native::CallVoid(s.methods.hide, s.object)); // retains official Alt/UI/platform checks
            }
            const auto root = s.root;
            s.root = 0;
            s.object = nullptr;
            Returned(s.references.Free(root));
            s.status = "cursor mode released";
        }
    } catch (const std::exception& e) {
        s.status = e.what();
    }
}
}
