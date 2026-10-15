#include <hksr/runtime/RuntimeHost.hpp>
#include "../../src/runtime/RuntimeStartup.hpp"
#include "../../src/runtime/RuntimeAvailability.hpp"
#include "../../src/runtime/RuntimeInitialization.hpp"
#include <Windows.h>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <thread>

namespace {
void Check(bool ok) {if(!ok) throw std::runtime_error("runtime host test failed");}
struct Startup {
    std::vector<std::string> calls;
    std::string failure,stop_at;
    bool stop=false,prepared=false,installed=false;
    bool Stopping() const {return stop;}
    void Step(const char* text) {
        calls.emplace_back(text);
        if(stop_at==text) stop=true;
        if(failure==text) throw std::runtime_error(text);
    }
    void Enter(){Step("enter");}
    void OpenMetadata(){Step("metadata");}
    void InstallObservers(){Step("observers");}
    void PrepareClient(){Step("prepare");}
    void InstallFrames(){Step("frames");}
    void Leave(){Step("leave");}
    void Error(const char*) noexcept {calls.emplace_back("error");}
    void Finish(bool p,bool i) noexcept {prepared=p;installed=i;calls.emplace_back("finish");}
};
struct Client: hksr::api::RuntimeClient {
    void Prepare(hksr::api::RuntimeMetadata&,hksr::api::RuntimeCapabilities) override {Check(false);}
    bool Frame(const hksr::api::WorldFrame&,bool) override {Check(false);return false;}
};
unsigned readiness=0;
void* __fastcall TestDomain(){return readiness?&readiness:nullptr;}
void** __fastcall TestAssemblies(void*,std::size_t* count) {
    static void* assembly=&readiness;
    if(readiness==3)RaiseException(0xE0170002,0,0,nullptr);
    *count=readiness==2?1:0;return &assembly;
}
}
void TestRuntimeHost() {
    using hksr::api::detail::ObserveDomain;
    using hksr::api::RuntimeAvailability;
    // Reproduce the actual missing boundary: assemblies may exist while the
    // official initializer still owns its lock and its count is already one.
    CRITICAL_SECTION initialization;InitializeCriticalSection(&initialization);
    CRITICAL_SECTION* slot=nullptr;int count=0;
    using hksr::api::detail::ObserveInitialization;
    Check(ObserveInitialization(&slot,&count)==RuntimeAvailability::WaitingInitialization);
    slot=&initialization;
    Check(ObserveInitialization(&slot,&count)==RuntimeAvailability::WaitingInitialization);
    EnterCriticalSection(&initialization);count=1;
    Check(ObserveInitialization(&slot,&count)==RuntimeAvailability::WaitingInitialization);
    RuntimeAvailability concurrent=RuntimeAvailability::Available;
    std::thread observer([&]{concurrent=ObserveInitialization(&slot,&count);});observer.join();
    Check(concurrent==RuntimeAvailability::WaitingInitialization);
    LeaveCriticalSection(&initialization);
    Check(ObserveInitialization(&slot,&count)==RuntimeAvailability::Available);
    Check(initialization.OwningThread==nullptr);
    count=0;Check(ObserveInitialization(&slot,&count)==RuntimeAvailability::WaitingInitialization);
    Check(ObserveInitialization(&slot,nullptr)==RuntimeAvailability::Unsupported);
    Check(initialization.OwningThread==nullptr);DeleteCriticalSection(&initialization);
    Check(ObserveDomain(nullptr,nullptr).state==RuntimeAvailability::WaitingTable);
    Check(ObserveDomain(TestDomain,TestAssemblies).state==RuntimeAvailability::WaitingDomain);
    readiness=1;Check(ObserveDomain(TestDomain,TestAssemblies).state==RuntimeAvailability::WaitingAssemblies);
    readiness=2;Check(ObserveDomain(TestDomain,TestAssemblies).state==RuntimeAvailability::Available);
    readiness=3;Check(ObserveDomain(TestDomain,TestAssemblies).state==RuntimeAvailability::Unsupported);
    Check(hksr::api::ObserveRuntimeAvailability(hksr::api::RuntimeVersion::Hsr450).state==RuntimeAvailability::WaitingModules);
    Check(hksr::api::ObserveRuntimeAvailability(hksr::api::RuntimeVersion::Hsr460).state==RuntimeAvailability::WaitingModules);
    Check(hksr::api::ObserveRuntimeAvailability().state==RuntimeAvailability::WaitingModules);
    Check(hksr::api::ObserveRuntimeAvailability(static_cast<hksr::api::RuntimeVersion>(99)).state==RuntimeAvailability::Unsupported);
    bool unsupported_host=false;
    try {auto subscription=hksr::api::RegisterRuntimeClient(std::make_shared<Client>());} catch(const std::exception&) {unsupported_host=true;}
    Check(unsupported_host);
    bool unsupported_thread=false;
    try {hksr::api::ResolveRuntimeThreadMethods(hksr::api::RuntimeVersion::Hsr460);} catch(const std::exception&) {unsupported_thread=true;}
    Check(unsupported_thread);
    using hksr::api::detail::RunRuntimeStartup;
    Startup normal;RunRuntimeStartup(normal);
    Check(normal.calls==std::vector<std::string>{"enter","metadata","observers","prepare","frames","leave","finish"});
    Check(normal.prepared && normal.installed);
    for(const auto* failure:{"enter","metadata","observers","prepare","frames"}) {
        Startup failed;failed.failure=failure;RunRuntimeStartup(failed);
        Check(!failed.installed && failed.calls[failed.calls.size()-2]=="leave");
        Check(failed.calls.back()=="finish");
    }
    Startup detach;detach.failure="leave";RunRuntimeStartup(detach);
    Check(detach.installed && detach.calls[detach.calls.size()-2]=="error");
    Startup cancelled;cancelled.stop=true;RunRuntimeStartup(cancelled);
    Check(cancelled.calls==std::vector<std::string>{"finish"});
    for(const auto* step:{"enter","metadata","observers","prepare","frames"}) {
        Startup cancelled_at;cancelled_at.stop_at=step;RunRuntimeStartup(cancelled_at);
        Check(cancelled_at.calls.back()=="finish");
        Check(cancelled_at.installed==(std::string(step)=="frames"));
    }
    // Real DLL worker in this test process: no Unity/GameAssembly is loaded,
    // therefore metadata/consumer preparation must not run. Not a game test.
    using namespace hksr::api;
    Check(!GetModuleHandleW(L"GameAssembly.dll") && !GetModuleHandleW(L"UnityPlayer.dll"));
    auto client=std::make_shared<Client>();std::weak_ptr<Client> life=client;
    const auto caller=GetCurrentThreadId();auto subscription=RegisterRuntimeClient(hksr::api::RuntimeVersion::Hsr460,client);client.reset();
    const auto deadline=GetTickCount64()+5000;
    while(!ReadRuntimeHostStatus().worker_finished && GetTickCount64()<deadline) Sleep(1);
    const auto state=ReadRuntimeHostStatus();
    Check(state.worker_finished && state.worker_thread!=caller && state.resident);
    Check(state.phase==RuntimeHostPhase::Failed && !state.world_hook && !state.startup_errors.empty() && life.expired());
    Check(subscription.Status().phase==RuntimeHostPhase::Failed && !subscription.Status().prepared);
    auto moved=std::move(subscription);Check(subscription.Status().phase==RuntimeHostPhase::Stopped);
    moved.RequestStop();Check(moved.Status().phase==RuntimeHostPhase::Failed);
    bool incompatible=false;try{auto rejected=RegisterRuntimeClient(hksr::api::RuntimeVersion::Hsr450,std::make_shared<Client>());}catch(const std::logic_error&){incompatible=true;}
    Check(incompatible);
    bool restarted=false;try{auto rejected=RegisterRuntimeClient(hksr::api::RuntimeVersion::Hsr460,std::make_shared<Client>());}catch(const std::logic_error&){restarted=true;}
    Check(restarted);
    std::cout<<"Runtime host tests passed: shared startup ordering/cancellation/failure and actual unsupported-process worker.\n";
}
