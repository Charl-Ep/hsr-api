#pragma once
#include <exception>

namespace hksr::api::detail {
// Shared ordering for the real worker and deterministic local tests. Optional
// hook failures are receipts, not a prerequisite for unrelated capabilities.
template <class Operations> void RunRuntimeStartup(Operations& ops) noexcept {
    bool prepared = false, installed = false;
    try {
        ops.Enter();
        ops.OpenMetadata();
        ops.InstallObservers();
        ops.PrepareClient();
        prepared = true;
        ops.InstallFrames();
        installed = true;
    } catch (const std::exception& e) {
        ops.Error(e.what());
    } catch (...) {
        ops.Error("unknown runtime startup failure");
    }
    // Enter may fail after a partial attachment; always attempt its paired exit.
    try {
        ops.Leave();
    } catch (const std::exception& e) {
        ops.Error(e.what());
    } catch (...) {
        ops.Error("unknown runtime thread exit failure");
    }
    // Installed hooks cannot be removed by pretending this worker is a
    // quiescent point. Publish only after preparation and worker attachment exit.
    ops.Finish(prepared, installed);
}
}
