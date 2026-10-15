#pragma once
#include <exception>

namespace hksr::sdk_v1::detail {
// Shared ordering for the real worker and deterministic local tests. Optional
// hook failures are receipts, not a prerequisite for unrelated capabilities.
template<class Operations> void RunRuntimeStartup(Operations& ops) noexcept {
    bool entered=false,prepared=false,installed=false;
    try {
        if(!ops.Stopping()) {
            entered=true; // even a partial Enter must reach same-thread Leave
            ops.Enter();
            if(!ops.Stopping()) {
                ops.OpenMetadata();
                if(!ops.Stopping()) {
                    ops.InstallObservers();
                    if(!ops.Stopping()) {
                        ops.PrepareClient(); prepared=true;
                        if(!ops.Stopping()) {ops.InstallFrames(); installed=true;}
                    }
                }
            }
        }
    } catch(const std::exception& e) {ops.Error(e.what());}
    catch(...) {ops.Error("unknown runtime startup failure");}
    if(entered) {
        try {ops.Leave();}
        catch(const std::exception& e) {ops.Error(e.what());}
        catch(...) {ops.Error("unknown runtime thread exit failure");}
    }
    // Installed hooks cannot be removed by pretending this worker is a
    // quiescent point. Publish only after preparation and worker attachment exit.
    ops.Finish(prepared,installed);
}
}
