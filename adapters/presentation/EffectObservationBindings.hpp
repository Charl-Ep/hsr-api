#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <array>
#include <functional>
namespace hksr::api::detail {
using EffectObservationFields = std::function<std::vector<DeclaredField>(const char*, const char*)>;
struct EffectScopeBindings {
    Method owner_mono, camera_plugin;
    std::size_t plugin_owner = 0, request_offset = 0, id_offset = 0;
    std::string behavior_error, plugin_error, camera_error;
};
struct EffectScopeEntries {
    std::array<Method, 8> methods{};
    std::array<bool, 3> supported{};
    std::vector<std::string> errors;
};
struct EffectSubmissionBindings {
    Method caster;
    std::size_t id_offset = 0, caster_offset = 0, config_offset = 0, context_offset = 0;
    std::string trigger_error;
};
struct EffectSubmissionEntries {
    std::array<void*, 3> methods{};
    std::vector<std::string> errors;
};
EffectScopeBindings ResolveEffectScopeBindings(MethodResolver&, const EffectObservationFields&);
EffectScopeEntries ResolveEffectScopeEntries(MethodResolver&);
std::size_t EffectScopeEntryGroup(std::size_t logical);
EffectSubmissionBindings ResolveEffectSubmissionBindings(MethodResolver&, const EffectObservationFields&);
EffectSubmissionEntries ResolveEffectSubmissionEntries(MethodResolver&);
}
