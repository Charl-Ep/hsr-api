#include "../../../src/presentation/audio/AudioPostRoute.hpp"
#include <Windows.h>
#include <iostream>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using namespace hksr::api;
using namespace hksr::api::detail;
void Check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class T> Method M(T value) {
    return {reinterpret_cast<void*>(value), nullptr};
}
struct NativeInfo {
    std::uint32_t playing = 0;
};
struct NativeEntry {
    AudioPostArguments args;
    std::uint32_t node = 0;
    bool locked = false, playing = false, closed = false;
};
struct Fixture;
struct Cookie {
    Fixture* owner = nullptr;
};
void ManagerCallback(void*, std::uint32_t, NativeInfo*);
struct CallbackObject {
    void (*entry)(void*, std::uint32_t, NativeInfo*) = &ManagerCallback;
};
struct Fixture {
    int name = 1, owned = 2, official = 3, old_art = 4, foreign_callback = 6, metadata = 8;
    CallbackObject callback;
    Cookie cookie{this};
    bool registered = true, current = true, reject_open = false, reject_enqueue = false, unknown_play = false,
         reject_close = false;
    unsigned original_posts = 0, opens = 0, enqueues = 0, plays = 0, stops = 0, closes = 0, ended = 0;
    std::uint32_t next_id = 100, next_logical = 1000;
    int next_root = 0, bank_usage = 0;
    std::map<int, void*> roots;
    std::map<std::uint32_t, NativeEntry> native;
    std::map<std::uint32_t, std::uint32_t> cache;
    std::vector<std::string> traces;
    AudioPostArguments last_original;
}* fixture = nullptr;
void ManagerCallback(void* cookie, std::uint32_t flags, NativeInfo* info) {
    auto& f = *static_cast<Cookie*>(cookie)->owner;
    Check(flags == 1 && info && f.cache.count(info->playing) == 1, "native End did not retain actual manager identity");
    f.cache.erase(info->playing);
    --f.bank_usage;
    ++f.ended;
    f.native.at(info->playing).playing = false;
}
int __fastcall Keep(void* object, int, int, void*) {
    const auto id = ++fixture->next_root;
    fixture->roots[id] = object;
    return id;
}
void* __fastcall Target(int id, void*) {
    return fixture->roots.at(id);
}
void __fastcall Free(int id, void*) {
    fixture->roots.erase(id);
}

// Deliberately noninline: every test enters the actual installed MinHook thunk.
__declspec(noinline) std::uint32_t __fastcall Original(void* name, void* emitter, std::uint32_t flags, void* callback,
                                                       void* cookie, void* metadata) {
    Check(GetLastError() == 1234, "transparent original LastError input");
    auto& f = *fixture;
    ++f.original_posts;
    f.last_original = {name, emitter, callback, cookie, metadata, flags};
    const auto id = ++f.next_id;
    f.native[id] = {f.last_original, 0, false, true, true};
    SetLastError(5678);
    return id;
}
std::uint32_t Post(void* emitter = nullptr, void* callback = nullptr) {
    auto& f = *fixture;
    SetLastError(1234);
    return Original(&f.name, emitter ? emitter : &f.owned, 0x85, callback ? callback : &f.callback, &f.cookie,
                    &f.metadata);
}
std::optional<std::uint32_t> Select(const AudioPostArguments& args) {
    auto& f = *fixture;
    SetLastError(9999);
    if (!f.registered || !f.current || args.emitter != &f.owned || args.callback != &f.callback ||
        args.event_name != &f.name)
        return {};
    return 700u;
}
AudioPostOperations Operations() {
    AudioPostOperations out;
    out.entry = M(Original);
    out.references = {M(Keep), M(Target), M(Free), {}, {}};
    out.open = [](const AudioPostArguments& args) {
        auto& f = *fixture;
        ++f.opens;
        if (f.reject_open)
            return AudioPostOpen{{true, 0}, 0};
        const auto id = ++f.next_id;
        f.native[id] = {args, 0, false, false, false};
        return AudioPostOpen{{true, 0}, id};
    };
    out.lock = [](std::uint32_t id) {
        auto& entry = fixture->native.at(id);
        entry.locked = true;
        return ObjectResult{{true, 0}, &entry};
    };
    out.enqueue = [](void* playlist, std::uint32_t node) {
        auto& f = *fixture;
        ++f.enqueues;
        auto& entry = *static_cast<NativeEntry*>(playlist);
        Check(entry.locked, "enqueue without playlist lock");
        if (f.reject_enqueue)
            return AudioPostOperation{{true, 0}, false};
        entry.node = node;
        return AudioPostOperation{{true, 0}, true};
    };
    out.unlock = [](std::uint32_t id) {
        auto& entry = fixture->native.at(id);
        Check(entry.locked, "playlist unlocked twice");
        entry.locked = false;
        return AudioPostOperation{{true, 0}, true};
    };
    out.play = [](std::uint32_t id) {
        auto& f = *fixture;
        ++f.plays;
        auto& entry = f.native.at(id);
        Check(!entry.locked && entry.node == 700, "play before exact enqueue/unlock");
        entry.playing = true;
        // The native operation really began, but its return outcome is unknown.
        return f.unknown_play ? AudioPostOperation{{false, 0xe0681234}, false} : AudioPostOperation{{true, 0}, true};
    };
    out.stop = [](std::uint32_t id) {
        ++fixture->stops;
        fixture->native.at(id).playing = false;
        return AudioPostOperation{{true, 0}, true};
    };
    out.close = [](std::uint32_t id) {
        auto& f = *fixture;
        ++f.closes;
        auto& entry = f.native.at(id);
        Check(!entry.locked, "close while playlist locked");
        if (f.reject_close)
            return AudioPostOperation{{true, 0}, false};
        Check(!entry.closed, "sequence closed twice");
        entry.closed = true;
        return AudioPostOperation{{true, 0}, true};
    };
    return out;
}
auto Trace() {
    return [](const std::string& text) { fixture->traces.push_back(text); };
}
std::uint32_t ManagerPost() {
    auto& f = *fixture;
    ++f.bank_usage;
    const auto logical = ++f.next_logical;
    const auto actual = Post();
    if (actual)
        Check(f.cache.emplace(actual, logical).second, "actual playing ID reused");
    else
        --f.bank_usage;
    return actual;
}
void NativeEnd(std::uint32_t actual) {
    auto& f = *fixture;
    auto& entry = f.native.at(actual);
    NativeInfo info{actual};
    Check(entry.args.callback == &f.callback && entry.args.cookie == &f.cookie,
          "original manager callback/cookie replaced");
    static_cast<CallbackObject*>(entry.args.callback)->entry(entry.args.cookie, 1, &info);
}
void Run() {
    {
        Fixture f;
        fixture = &f;
        AudioPostRoute route(Operations(), Select, Trace());
        f.registered = false;
        Check(Post() != 0 && GetLastError() == 5678, "unregistered actor not transparent");
        f.registered = true;
        Check(Post(&f.official) != 0 && GetLastError() == 5678, "official actor not transparent");
        Check(Post(nullptr, &f.foreign_callback) != 0, "foreign callback not transparent");
        Check(Post(&f.old_art) != 0, "old model Art not transparent");
        f.current = false;
        Check(Post() != 0, "stale model generation not transparent");
        Check(f.original_posts == 5 && f.opens == 0, "scope miss opened a dynamic sequence");
        Check(f.last_original.flags == 0x85 && f.last_original.cookie == &f.cookie &&
                  f.last_original.metadata == &f.metadata,
              "forwarded ABI changed");
        route.Stop();
        f.current = true;
        Check(Post() != 0 && f.original_posts == 6, "withdrawn route not transparent");
        Check(route.Collect() && f.roots.empty(), "empty route did not close");
    }
    {
        Fixture f;
        fixture = &f;
        AudioPostRoute route(Operations(), Select, Trace());
        const auto actual = ManagerPost();
        const auto& entry = f.native.at(actual);
        Check(actual != 0 && actual == f.next_id && f.original_posts == 0, "route did not return actual dynamic ID");
        Check(entry.args.flags == 0x85 && entry.args.callback == &f.callback && entry.args.cookie == &f.cookie,
              "caller callback contract changed");
        Check(entry.node == 700 && entry.playing && entry.closed, "single selected node not playing after Close");
        Check(f.bank_usage == 1 && f.cache.count(actual) == 1, "manager cache/bank not registered with actual ID");
        Check(route.Collect() && f.roots.empty() && f.closes == 1, "successful post roots or close retained");
        NativeEnd(actual);
        Check(f.bank_usage == 0 && f.cache.empty() && f.ended == 1,
              "native End did not release original manager banks");
        route.Stop();
        Check(route.Collect(), "matched route failed shutdown");
    }
    {
        Fixture f;
        fixture = &f;
        f.reject_open = true;
        AudioPostRoute route(Operations(), Select, Trace());
        Check(ManagerPost() == 0 && f.original_posts == 0, "rejected node must not post original fallback");
        Check(route.Collect() && f.roots.empty() && f.bank_usage == 0,
              "known Open rejection retained uncertain receipt");
        Check(f.enqueues == 0 && f.plays == 0, "Open rejection continued posting");
    }
    {
        Fixture f;
        fixture = &f;
        f.unknown_play = true;
        AudioPostRoute route(Operations(), Select, Trace());
        const auto actual = ManagerPost();
        Check(actual != 0 && f.cache.count(actual) == 1 && f.bank_usage == 1,
              "unknown Play lost actual ID/bank ownership");
        Check(f.native.at(actual).playing && f.plays == 1, "unknown Play fixture did not create its side effect");
        Check(route.Collect() && f.stops == 1 && f.closes == 1 && !f.native.at(actual).playing,
              "known playing identity did not drain after unknown Play");
        Check(f.plays == 1 && f.original_posts == 0, "unknown Play was retried or double posted");
        NativeEnd(actual);
        Check(f.bank_usage == 0 && f.roots.empty(), "unknown Play lost original completion contract");
    }
    {
        Fixture f;
        fixture = &f;
        f.reject_enqueue = true;
        AudioPostRoute route(Operations(), Select, Trace());
        const auto actual = ManagerPost();
        Check(actual != 0 && f.plays == 0 && f.original_posts == 0, "enqueue failure played or fell through");
        Check(route.Collect() && f.stops == 1 && f.closes == 1 && f.roots.empty(),
              "enqueue failure did not retire its exact sequence");
        NativeEnd(actual);
        Check(f.bank_usage == 0, "enqueue failure did not retain manager release path");
    }
    {
        Fixture f;
        fixture = &f;
        f.reject_close = true;
        AudioPostRoute route(Operations(), Select, Trace());
        const auto actual = ManagerPost();
        Check(actual != 0 && f.bank_usage == 1, "Close failure discarded actual ID");
        Check(!route.Collect() && !f.traces.empty(), "pending Close failure has no diagnostic");
        const auto traces = f.traces.size();
        Check(!route.Collect() && f.traces.size() == traces, "same pending failure spammed diagnostics");
        f.reject_close = false;
        Check(route.Collect() && f.roots.empty(), "known Close rejection did not finish after recovery");
        NativeEnd(actual);
        Check(f.bank_usage == 0, "Close recovery lost native End ownership");
    }
    {
        Fixture f;
        fixture = &f;
        f.reject_close = true;
        auto operations = Operations();
        const auto close = operations.close;
        std::uint32_t nested = 0;
        bool reentered = false;
        operations.close = [&](std::uint32_t id) {
            if (!f.reject_close && !reentered) {
                reentered = true;
                nested = ManagerPost();
            }
            return close(id);
        };
        AudioPostRoute route(std::move(operations), Select, Trace());
        const auto outer = ManagerPost();
        f.reject_close = false;
        Check(!route.Collect() && nested != 0, "reentrant post was lost from pending ledger");
        Check(route.Collect() && f.roots.empty() && f.plays == 2, "reentrant collection corrupted receipts");
        NativeEnd(outer);
        NativeEnd(nested);
        Check(f.bank_usage == 0 && f.cache.empty(), "reentrant manager identities mixed");
    }
}
}
int main() {
    try {
        Run();
        std::cout << "Audio PostEvent route tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
