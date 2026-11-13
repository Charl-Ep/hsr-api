#include "AudioPostRoute.hpp"
#include "../../../adapters/presentation/AudioPostBindings.hpp"
#include "../../platform/NativeHooks.hpp"
#include <hksr/managed/ManagedRoots.hpp>
#include <Windows.h>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>
#include <list>
#include <sstream>

namespace hksr::api::detail {
struct AudioPostRoute::State {
    struct Pending {
        AudioPostOperations operations;
        ManagedRoots roots;
        std::vector<ManagedRootId> held;
        std::uint32_t id = 0;
        bool attempted = false, locked = false, playing = false, closed = false, cancel = false, submitting = true,
             open_unknown = false;
        bool unlock_unknown = false, close_unknown = false;
        Trace trace;
        bool reported = false;
        explicit Pending(AudioPostOperations value, Trace log)
            : operations(std::move(value)), roots(operations.references), trace(std::move(log)) {
            held.reserve(5);
        }
        void Failure(const char* stage, const AudioPostOperation& result) noexcept {
            if (reported)
                return;
            reported = true;
            try {
                std::ostringstream text;
                text << "audio node " << stage << " failed id=" << id << " returned=" << result.call.returned
                     << " accepted=" << result.accepted << " code=" << result.native_code
                     << " exception=" << result.call.exception;
                if (trace)
                    trace(text.str());
            } catch (...) {
            }
        }
        void Keep(void* object) {
            if (!object)
                return;
            const auto key = roots.Keep(object);
            held.push_back(key);
            if (!roots.Status(key).ready)
                throw std::runtime_error("audio post retention failed");
        }
        bool Release() {
            bool done = true;
            for (auto it = held.begin(); it != held.end();)
                if (roots.Release(*it).complete) {
                    roots.Forget(*it);
                    it = held.erase(it);
                } else {
                    done = false;
                    ++it;
                }
            return done;
        }
        bool Service(bool retiring = false) {
            if (open_unknown && !id)
                return false; // No returned identity means no safe cleanup target.
            if (locked) {
                if (unlock_unknown)
                    return false;
                const auto value = operations.unlock(id);
                if (!value.call.returned) {
                    unlock_unknown = value.call.exception != 0;
                    Failure("unlock", value);
                    return false;
                }
                if (!value.accepted) {
                    Failure("unlock", value);
                    return false;
                }
                locked = false;
            }
            if (id && !closed) {
                if (retiring || cancel || !playing) {
                    const auto value = operations.stop(id);
                    if (!value.call.returned || !value.accepted) {
                        Failure("stop", value);
                        return false;
                    }
                    playing = false;
                }
                if (close_unknown)
                    return false;
                const auto value = operations.close(id);
                if (!value.call.returned) {
                    close_unknown = value.call.exception != 0;
                    Failure("close", value);
                    return false;
                }
                if (!value.accepted) {
                    Failure("close", value);
                    return false;
                }
                closed = true;
            }
            return Release();
        }
        std::uint32_t Submit(const AudioPostArguments& args, std::uint32_t node) {
            Keep(args.event_name);
            Keep(args.emitter);
            Keep(args.callback);
            Keep(args.cookie);
            attempted = true;
            const auto opened = operations.open(args);
            id = opened.id;
            if (!opened.call.returned || !id) {
                open_unknown = !opened.call.returned && opened.call.exception != 0;
                cancel = true;
                Failure("open", {opened.call, false});
                return id;
            }
            const auto list = operations.lock(id);
            if (!list.call.returned || !list.object) {
                cancel = true;
                Failure("lock", {list.call, false});
                return id;
            }
            locked = true;
            Keep(list.object);
            const auto enqueued = operations.enqueue(list.object, node);
            const auto unlocked = operations.unlock(id);
            if (!unlocked.call.returned || !unlocked.accepted) {
                unlock_unknown = !unlocked.call.returned && unlocked.call.exception != 0;
                cancel = true;
                Failure("unlock", unlocked);
                return id;
            }
            locked = false;
            if (!enqueued.call.returned || !enqueued.accepted) {
                cancel = true;
                Failure("enqueue", enqueued);
                return id;
            }
            const auto started = operations.play(id);
            if (!started.call.returned || !started.accepted) {
                cancel = true;
                Failure("play", started);
                return id;
            }
            playing = true;
            // Return the actual ID even if Close needs another service attempt:
            // original AudioManager must retain its real cache/bank/callback.
            Service();
            return id;
        }
    };
    struct Router {
        NativeHooks hooks;
        std::mutex mutex;
        void* target = nullptr;
        using Clients = std::vector<std::shared_ptr<State>>;
        std::shared_ptr<const Clients> clients = std::make_shared<const Clients>();
        static Router& Get() {
            static auto* value = new Router;
            return *value;
        }
    };
    AudioPostOperations operations;
    Select select;
    Trace trace;
    std::atomic<bool> active{true};
    DWORD thread = GetCurrentThreadId();
    std::list<std::unique_ptr<Pending>> pending;
    std::shared_ptr<State> retained_self;
    bool collecting = false;
    State(AudioPostOperations o, Select s, Trace t)
        : operations(std::move(o)), select(std::move(s)), trace(std::move(t)) {}
    void Log(const char* text) noexcept {
        try {
            if (trace)
                trace(text);
        } catch (...) {
        }
    }
    static std::uint32_t Invoke(const AudioPostArguments& args) {
        const auto last_error = GetLastError();
        auto& router = Router::Get();
        const auto clients = std::atomic_load(&router.clients);
        for (const auto& state : *clients) {
            if (!state->active.load() || state->thread != GetCurrentThreadId())
                continue;
            Pending* call = nullptr;
            try {
                const auto node = state->select(args);
                if (!node || !*node)
                    continue;
                state->pending.push_back(std::make_unique<Pending>(state->operations, state->trace));
                call = state->pending.back().get();
                const auto id = call->Submit(args, *node);
                call->submitting = false;
                return id;
            } catch (const std::exception& e) {
                state->Log(e.what());
                if (call) {
                    call->submitting = false;
                    call->cancel = true;
                    if (call->attempted)
                        return call->id;
                }
            } catch (...) {
                state->Log("audio post route failed");
                if (call) {
                    call->submitting = false;
                    call->cancel = true;
                    if (call->attempted)
                        return call->id;
                }
            }
        }
        SetLastError(last_error);
        return ForwardAudioPost(router.hooks.Original(0), args);
    }
};
AudioPostRoute::AudioPostRoute(AudioPostOperations operations, Select select, Trace trace) {
    if (!operations.entry.code || !select)
        throw std::invalid_argument("audio post route requires entry and selection");
    auto state = std::make_shared<State>(std::move(operations), std::move(select), std::move(trace));
    auto& router = State::Router::Get();
    std::lock_guard<std::mutex> lock(router.mutex);
    if (router.target && router.target != state->operations.entry.code)
        throw std::runtime_error("audio post route entry differs");
    if (!router.target) {
        router.hooks.Install(
            {{"owned audio PostEvent", state->operations.entry.code, AudioPostThunk<State>::Address()}});
        router.target = state->operations.entry.code;
    }
    auto clients = std::make_shared<State::Router::Clients>(*std::atomic_load(&router.clients));
    clients->push_back(state);
    std::atomic_store(&router.clients, std::shared_ptr<const State::Router::Clients>(std::move(clients)));
    state_ = std::move(state);
}
AudioPostRoute::~AudioPostRoute() {
    Stop();
    if (state_ && !state_->pending.empty())
        state_->retained_self = state_;
}
void AudioPostRoute::Stop() noexcept {
    if (!state_)
        return;
    state_->active.store(false);
    try {
        auto& router = State::Router::Get();
        std::lock_guard<std::mutex> lock(router.mutex);
        auto clients = std::make_shared<State::Router::Clients>();
        for (const auto& client : *std::atomic_load(&router.clients))
            if (client != state_)
                clients->push_back(client);
        std::atomic_store(&router.clients, std::shared_ptr<const State::Router::Clients>(std::move(clients)));
    } catch (...) {
    }
}
bool AudioPostRoute::Collect() {
    if (!state_)
        return true;
    if (state_->collecting)
        return false;
    state_->collecting = true;
    struct Guard {
        bool& value;
        ~Guard() {
            value = false;
        }
    } guard{state_->collecting};
    std::list<std::unique_ptr<State::Pending>> batch;
    batch.splice(batch.end(), state_->pending);
    for (auto it = batch.begin(); it != batch.end();) {
        if ((*it)->submitting) {
            ++it;
            continue;
        }
        try {
            if ((*it)->Service(!state_->active.load())) {
                it = batch.erase(it);
                continue;
            }
        } catch (const std::exception& e) {
            if (!(*it)->reported) {
                (*it)->reported = true;
                state_->Log(e.what());
            }
        }
        ++it;
    }
    state_->pending.splice(state_->pending.end(), batch);
    if (state_->pending.empty())
        state_->retained_self.reset();
    return state_->pending.empty();
}
}
