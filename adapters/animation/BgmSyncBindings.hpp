#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <functional>
namespace hksr::api::detail {
struct BgmSyncInputType {
    RuntimeType kind;
    void* system = nullptr;
};
struct BgmSyncReferenceField {
    void* field = nullptr;
    Method set;
};
struct BgmSyncBindings {
    void *model_type = nullptr, *component_type = nullptr;
    BgmSyncInputType config_type, chapter_type, phase_type;
    BgmSyncReferenceField state_name, chapters, cue, phases, double_phases;
    Method start, stop, animator, controller;
    std::size_t helper_offset = 0, config_offset = 0, bpm_offset = 0, time_offset = 0, blend_offset = 0,
                normalized_offset = 0;
};
using BgmSyncType = std::function<BgmSyncInputType(const char*, const char*)>;
using BgmSyncReference = std::function<BgmSyncReferenceField(const BgmSyncInputType&, const char*, const char*)>;
BgmSyncBindings ResolveBgmSyncBindings(RuntimeMetadata&, const BgmSyncType&, const BgmSyncReference&);
}
