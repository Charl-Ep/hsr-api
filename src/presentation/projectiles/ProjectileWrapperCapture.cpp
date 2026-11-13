#include "ProjectileWrapperCapture.hpp"
#include "../../../adapters/presentation/ProjectileHookBridge.hpp"
#include "../../../adapters/presentation/ProjectileWrapperBindings.hpp"
#include "../../platform/NativeHooks.hpp"
#include <hksr/native/MetadataReader.hpp>
#include <hksr/native/Invocation.hpp>
#include <Windows.h>
#include <cstring>
#include <mutex>
#include <stdexcept>

namespace hksr::api::detail {
struct ProjectileWrapperCapture::State {
    struct Router {
        NativeHooks hooks;
        std::mutex mutex;
        void* factory = nullptr;
        void* constructor = nullptr;
        Method active;
    };
    static Router& Route() {
        static auto* value = new Router;
        return *value;
    }
    static thread_local State* current;
    static thread_local State* wrapping;
    State* previous = nullptr;
    void* carrier = nullptr;
    OwnedUnityObjects objects;
    std::vector<UnityObjectId> ids;
    std::vector<void*> values;
    bool complete = true, active = false;
    State(ReferenceMethods refs, UnityLifetimeMethods methods) : objects(methods, refs) {}
    void Capture(void* object) noexcept {
        try {
            ids.reserve(ids.size() + 1);
            values.reserve(values.size() + 1);
            ids.push_back(objects.Adopt(object));
            values.push_back(object);
            if (!objects.Status(ids.back()).root.ready)
                complete = false;
        } catch (...) {
            complete = false;
        }
    }
    static void* Factory(void* carrier, void* effect, void* metadata) {
        auto* original = Route().hooks.Original(0);
        State* saved = wrapping;
        wrapping = current && current->carrier == carrier ? current : nullptr;
        __try {
            return ForwardProjectileWrapperFactory(original, carrier, effect, metadata);
        } __finally {
            wrapping = saved;
        }
    }
    static void Constructor(void* object, void* name, void* metadata) {
        auto* original = Route().hooks.Original(1);
        State* saved = wrapping;
        const DWORD error = GetLastError();
        wrapping = nullptr; // constructors/plugins nested in this ctor are not the wrapper
        if (saved)
            saved->Capture(object);
        SetLastError(error);
        __try {
            ForwardProjectileWrapperConstruction(original, object, name, metadata);
        } __finally {
            wrapping = saved;
        }
    }
};
thread_local ProjectileWrapperCapture::State* ProjectileWrapperCapture::State::current = nullptr;
thread_local ProjectileWrapperCapture::State* ProjectileWrapperCapture::State::wrapping = nullptr;
void ProjectileWrapperCapture::Prepare(MethodResolver& resolver) {
    auto& r = State::Route();
    std::lock_guard<std::mutex> lock(r.mutex);
    const auto binding = ResolveProjectileWrapperBindings(resolver);
    const auto factory = binding.factory;
    const auto ctor = binding.constructor;
    const auto active = binding.active;
    if (r.factory || r.constructor) {
        if (r.factory != factory.code || r.constructor != ctor)
            throw std::runtime_error("projectile wrapper capture domain changed");
        return;
    }
    r.hooks.Install({{"projectile wrapper factory", factory.code,
                      reinterpret_cast<void*>(&ProjectileWrapperEntry<&State::Factory>)},
                     {"projectile wrapper birth", ctor,
                      reinterpret_cast<void*>(&ProjectileConstructionEntry<&State::Constructor>)}});
    r.factory = factory.code;
    r.constructor = ctor;
    r.active = active;
}
ProjectileWrapperCapture::ProjectileWrapperCapture(ReferenceMethods refs, UnityLifetimeMethods methods)
    : state_(std::make_shared<State>(refs, methods)) {}
ProjectileWrapperCapture::~ProjectileWrapperCapture() {
    End();
}
void ProjectileWrapperCapture::Begin(void* carrier) {
    if (state_->active)
        throw std::logic_error("projectile wrapper capture already active");
    state_->carrier = carrier;
    state_->previous = State::current;
    State::current = state_.get();
    state_->active = true;
}
void ProjectileWrapperCapture::End() noexcept {
    if (state_->active) {
        State::current = state_->previous;
        state_->active = false;
    }
}
const std::vector<void*>& ProjectileWrapperCapture::Objects() const {
    return state_->values;
}
bool ProjectileWrapperCapture::Complete() const {
    return state_->complete;
}
bool ProjectileWrapperCapture::Stop() {
    bool complete = true;
    for (auto it = state_->ids.begin(); it != state_->ids.end();) {
        const auto value = state_->objects.ObserveEnd(*it);
        if (value.complete) {
            state_->objects.Forget(*it);
            it = state_->ids.erase(it);
            continue;
        }
        if (!value.native_destroyed) {
            if (!value.alive.read.returned)
                complete = false;
            else if (value.alive.value && !native::CallVoid(State::Route().active, value.target.object, false).returned)
                complete = false;
        }
        ++it;
    }
    return complete;
}
bool ProjectileWrapperCapture::Drain(bool destroy) {
    if (state_->active)
        return false;
    if (destroy && !Stop())
        return false;
    for (auto it = state_->ids.begin(); it != state_->ids.end();) {
        const auto value = destroy ? state_->objects.Close(*it) : state_->objects.ObserveEnd(*it);
        if (value.complete) {
            state_->objects.Forget(*it);
            it = state_->ids.erase(it);
        } else
            ++it;
    }
    return state_->complete && state_->ids.empty();
}
}
