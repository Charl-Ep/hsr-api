#include <hksr/animation/ActorBgmSync.hpp>
#include "../../src/animation/ActorBgmSyncCalls.hpp"
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace hksr::api;
namespace {
void Check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void Reject(F fn) {
    bool caught = false;
    try {
        fn();
    } catch (const std::exception&) {
        caught = true;
    }
    Check(caught, "invalid BGM config accepted");
}
template <class F> Method M(F fn) {
    return {reinterpret_cast<void*>(fn), nullptr};
}
struct Helper {
    void* config = nullptr;
};
struct Component {
    Helper* helper = nullptr;
    unsigned starts = 0, stops = 0;
};
void __fastcall Start(Component* component, void* config, void*) {
    component->helper->config = config;
    ++component->starts;
}
void __fastcall Stop(Component* component, void*) {
    component->helper->config = nullptr;
    ++component->stops;
}
void NativeCalls() {
    int first = 0, second = 0;
    Helper helper;
    Component component{&helper};
    Check(detail::StartBgmSync(M(Start), &component, &first).returned && helper.config == &first &&
              component.starts == 1,
          "BGM start ABI differs");
    const auto a = detail::ReadBgmSyncSlot(&component, offsetof(Component, helper), offsetof(Helper, config));
    Check(detail::OwnsBgmSyncSlot(a, &helper, &first), "owned helper/config not observed");
    Check(detail::StartBgmSync(M(Start), &component, &second).returned, "replacement start failed");
    const auto displaced = detail::StopOwnedBgmSync(M(Stop), &component, &helper, &first, offsetof(Component, helper),
                                                    offsetof(Helper, config));
    Check(displaced.call.returned && !displaced.matching && component.stops == 0 && helper.config == &second,
          "old request stopped another config");
    Helper replacement{&second};
    component.helper = &replacement;
    const auto replaced = detail::StopOwnedBgmSync(M(Stop), &component, &helper, &second, offsetof(Component, helper),
                                                   offsetof(Helper, config));
    Check(!replaced.matching && component.stops == 0, "old helper identity stopped replacement");
    const auto current = detail::StopOwnedBgmSync(M(Stop), &component, &replacement, &second,
                                                  offsetof(Component, helper), offsetof(Helper, config));
    Check(current.call.returned && current.matching && !current.active_after && component.stops == 1 &&
              !replacement.config,
          "own stop not observed");
}
BgmSyncConfig Config() {
    return {"State_Combat_Robin", 100, {{"Robin_BGM_LoopStart", {{0, .5f, .25f}, {4, .8f, .1f}}, std::nullopt}}};
}
void PublicCalls() {
    bool live = true, active = false, closed = false;
    unsigned starts = 0, stops = 0;
    BgmSyncConfig copied;
    ActorBgmSync client({[&](const auto& input) {
                             copied = input;
                             ++starts;
                             active = true;
                             return BgmSyncStartResult{BgmSyncStartState::Accepted, {7}, {}};
                         },
                         [&](auto token) { return BgmSyncObservation{token.value == 7, active, !active, {}}; },
                         [&](auto token) {
                             Check(token.value == 7, "token changed");
                             active = false;
                             ++stops;
                             return true;
                         },
                         [&] { return live; },
                         [&] {
                             closed = true;
                             return !active;
                         }});
    const auto result = client.Start(Config());
    Check(result.state == BgmSyncStartState::Accepted && starts == 1 && client.Observe(result.token).active &&
              !copied.chapters.front().double_speed_phases,
          "configuration or null x2 phase array changed");
    auto bad = Config();
    bad.bpm = 0;
    Reject([&] { client.Start(bad); });
    bad = Config();
    bad.chapters[0].phases[0].blend = std::numeric_limits<float>::infinity();
    Reject([&] { client.Start(bad); });
    Check(starts == 1, "invalid input reached native provider");
    live = false;
    Check(client.Start(Config()).state == BgmSyncStartState::Failed && starts == 1,
          "stale actor submitted new synchronization");
    Check(client.Stop(result.token) && stops == 1 && client.Close() && closed,
          "retired client did not drain own request");
}
}
int main() try {
    NativeCalls();
    PublicCalls();
    std::cout << "BGM sync typed calls, helper/config ownership and actor-bound configuration passed\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
}
