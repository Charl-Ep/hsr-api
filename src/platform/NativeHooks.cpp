#include "NativeHooks.hpp"
#include <MinHook.h>
#include <stdexcept>

namespace hksr::sdk_v1::detail {
bool NativeHooks::Pending() const {
    for (const auto& hook : hooks_) if (hook.created) return true;
    return false;
}
void NativeHooks::Remove() {
    std::string errors;
    for (auto& hook : hooks_) {
        if (!hook.created) continue;
        const auto disabled = MH_DisableHook(hook.target);
        if (disabled != MH_OK && disabled != MH_ERROR_DISABLED) {
            errors += "disable " + hook.name + ": " + MH_StatusToString(disabled) + "\n";
            continue;
        }
        const auto removed = MH_RemoveHook(hook.target);
        if (removed == MH_OK) { hook.created = false; hook.original = nullptr; }
        else errors += "remove " + hook.name + ": " + MH_StatusToString(removed) + "\n";
    }
    // Unresolved entries must not be mistaken for permission to unload the DLL.
    if (!errors.empty()) throw std::runtime_error(errors);
    hooks_.clear();
}
void NativeHooks::Install(std::initializer_list<HookSpec> specs) {
    if (Pending()) throw std::logic_error("native hook group already installed or needs removal");
    hooks_.clear(); hooks_.reserve(specs.size());
    for (const auto& spec : specs) hooks_.push_back({spec.name, spec.target, spec.detour});
    const auto initialized = MH_Initialize();
    if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED)
        throw std::runtime_error(std::string("MinHook initialization: ") + MH_StatusToString(initialized));
    std::string errors;
    for (auto& hook : hooks_) {
        const auto result = MH_CreateHook(hook.target, hook.detour, &hook.original);
        if (result == MH_OK) hook.created = true;
        else errors += "create " + hook.name + ": " + MH_StatusToString(result) + "\n";
    }
    bool enable_attempted=false;
    if (errors.empty()) {
        for (const auto& hook : hooks_) {
            const auto result = MH_QueueEnableHook(hook.target);
            if (result != MH_OK) errors += "queue " + hook.name + ": " + MH_StatusToString(result) + "\n";
        }
        if (errors.empty()) {
            enable_attempted=true;
            const auto result = MH_ApplyQueued();
            if (result != MH_OK) errors += std::string("enable hook group: ") + MH_StatusToString(result);
        }
    }
    if (!errors.empty()) {
        // ApplyQueued can partially enable a group. A worker cannot establish
        // engine quiescence merely because enable returned an error. Preserve
        // the trampolines/state; removal remains an explicit quiescent action.
        if(!enable_attempted) {
            try { Remove(); }
            catch (const std::exception& error) { errors += "rollback: "; errors += error.what(); }
        } else errors += "\nhook enable attempted; group retained until an explicit quiescent removal";
        throw std::runtime_error(errors);
    }
}
}
