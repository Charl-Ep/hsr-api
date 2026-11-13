#include <hksr/runtime/RuntimeHost.hpp>
#include "../../src/runtime/RuntimeStartup.hpp"
#include "../../adapters/runtime/RuntimeDomain.hpp"
#include "../../src/runtime/RuntimeInitialization.hpp"
#include <Windows.h>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <thread>

namespace {
void Check(bool ok) {
    if (!ok)
        throw std::runtime_error("runtime host test failed");
}
struct Startup {
    std::vector<std::string> calls;
    std::string failure;
    bool prepared = false, installed = false;
    void Step(const char* text) {
        calls.emplace_back(text);
        if (failure == text)
            throw std::runtime_error(text);
    }
    void Enter() {
        Step("enter");
    }
    void OpenMetadata() {
        Step("metadata");
    }
    void InstallObservers() {
        Step("observers");
    }
    void PrepareClient() {
        Step("prepare");
    }
    void InstallFrames() {
        Step("frames");
    }
    void Leave() {
        Step("leave");
    }
    void Error(const char*) noexcept {
        calls.emplace_back("error");
    }
    void Finish(bool p, bool i) noexcept {
        prepared = p;
        installed = i;
        calls.emplace_back("finish");
    }
};
struct Client : hksr::api::RuntimeClient {
    void Prepare(hksr::api::RuntimeMetadata&, hksr::api::RuntimeCapabilities) override {
        Check(false);
    }
    bool Frame(const hksr::api::WorldFrame&, bool) override {
        Check(false);
        return false;
    }
};
unsigned readiness = 0;
void* __fastcall TestDomain() {
    return readiness ? &readiness : nullptr;
}
void** __fastcall TestAssemblies(void*, std::size_t* count) {
    static void* assembly = &readiness;
    if (readiness == 3)
        RaiseException(0xE0170002, 0, 0, nullptr);
    *count = readiness == 2 ? 1 : 0;
    return &assembly;
}
}
void TestRuntimeHost() {
    using hksr::api::RuntimeAvailability;
    using hksr::api::detail::ObserveDomain;
    // Reproduce the actual missing boundary: assemblies may exist while the
    // official initializer still owns its lock and its count is already one.
    CRITICAL_SECTION initialization;
    InitializeCriticalSection(&initialization);
    CRITICAL_SECTION* slot = nullptr;
    int count = 0;
    using hksr::api::detail::ObserveInitialization;
    Check(ObserveInitialization(&slot, &count) == RuntimeAvailability::WaitingInitialization);
    slot = &initialization;
    Check(ObserveInitialization(&slot, &count) == RuntimeAvailability::WaitingInitialization);
    EnterCriticalSection(&initialization);
    count = 1;
    Check(ObserveInitialization(&slot, &count) == RuntimeAvailability::WaitingInitialization);
    RuntimeAvailability concurrent = RuntimeAvailability::Available;
    std::thread observer([&] { concurrent = ObserveInitialization(&slot, &count); });
    observer.join();
    Check(concurrent == RuntimeAvailability::WaitingInitialization);
    LeaveCriticalSection(&initialization);
    Check(ObserveInitialization(&slot, &count) == RuntimeAvailability::Available);
    Check(initialization.OwningThread == nullptr);
    count = 0;
    Check(ObserveInitialization(&slot, &count) == RuntimeAvailability::WaitingInitialization);
    Check(ObserveInitialization(&slot, nullptr) == RuntimeAvailability::Unsupported);
    Check(initialization.OwningThread == nullptr);
    DeleteCriticalSection(&initialization);
    Check(ObserveDomain(nullptr, nullptr).state == RuntimeAvailability::WaitingTable);
    Check(ObserveDomain(TestDomain, TestAssemblies).state == RuntimeAvailability::WaitingDomain);
    readiness = 1;
    Check(ObserveDomain(TestDomain, TestAssemblies).state == RuntimeAvailability::WaitingAssemblies);
    readiness = 2;
    Check(ObserveDomain(TestDomain, TestAssemblies).state == RuntimeAvailability::Available);
    readiness = 3;
    Check(ObserveDomain(TestDomain, TestAssemblies).state == RuntimeAvailability::Unsupported);
    Check(hksr::api::ObserveRuntimeAvailability().state == RuntimeAvailability::WaitingModules);
    bool unsupported_thread = false;
    try {
        hksr::api::ResolveRuntimeThreadMethods();
    } catch (const std::exception&) {
        unsupported_thread = true;
    }
    Check(unsupported_thread);
    using hksr::api::detail::RunRuntimeStartup;
    Startup normal;
    RunRuntimeStartup(normal);
    Check(normal.calls ==
          std::vector<std::string>{"enter", "metadata", "observers", "prepare", "frames", "leave", "finish"});
    Check(normal.prepared && normal.installed);
    const std::vector<std::string> startup_steps{"enter", "metadata", "observers", "prepare", "frames"};
    for (std::size_t i = 0; i < startup_steps.size(); ++i) {
        Startup failed;
        failed.failure = startup_steps[i];
        RunRuntimeStartup(failed);
        std::vector<std::string> expected(startup_steps.begin(), startup_steps.begin() + i + 1);
        expected.insert(expected.end(), {"error", "leave", "finish"});
        Check(failed.calls == expected && !failed.installed);
        Check(failed.prepared == (i == startup_steps.size() - 1));
    }
    Startup detach;
    detach.failure = "leave";
    RunRuntimeStartup(detach);
    Check(detach.installed && detach.calls[detach.calls.size() - 2] == "error");
    // Real DLL worker in this test process: no Unity/GameAssembly is loaded,
    // therefore metadata/consumer preparation must not run. Not a game test.
    using namespace hksr::api;
    Check(!GetModuleHandleW(L"GameAssembly.dll") && !GetModuleHandleW(L"UnityPlayer.dll"));
    auto client = std::make_shared<Client>();
    std::weak_ptr<Client> life = client;
    const auto caller = GetCurrentThreadId();
    auto subscription = RegisterRuntimeClient(client);
    client.reset();
    const auto deadline = GetTickCount64() + 5000;
    while (!ReadRuntimeHostStatus().worker_finished && GetTickCount64() < deadline)
        Sleep(1);
    const auto state = ReadRuntimeHostStatus();
    Check(state.worker_finished && state.worker_thread != caller && state.resident);
    Check(state.phase == RuntimeHostPhase::Failed && !state.world_hook && !state.startup_errors.empty() &&
          life.expired());
    Check(subscription.Status().phase == RuntimeHostPhase::Failed && !subscription.Status().prepared);
    auto moved = std::move(subscription);
    Check(subscription.Status().phase == RuntimeHostPhase::Stopped);
    moved.RequestStop();
    Check(moved.Status().phase == RuntimeHostPhase::Failed);
    bool failed_host = false;
    try {
        auto rejected = RegisterRuntimeClient(std::make_shared<Client>());
    } catch (const std::logic_error& error) {
        failed_host = std::string(error.what()).find("initialization failed") != std::string::npos;
    }
    Check(failed_host);
    bool restarted = false;
    try {
        auto rejected = RegisterRuntimeClient(std::make_shared<Client>());
    } catch (const std::logic_error&) {
        restarted = true;
    }
    Check(restarted);
    std::cout << "Runtime host tests passed: shared startup ordering/failure and actual "
                 "unsupported-process worker.\n";
}
