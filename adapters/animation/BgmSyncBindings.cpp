#include "BgmSyncBindings.hpp"
#include <stdexcept>
namespace hksr::api::detail {
namespace {
RuntimeField Field(RuntimeMetadata& m, RuntimeType owner, const char* name, const char* type,
                   std::size_t* offset = nullptr) {
    RuntimeField result;
    for (const auto& row : m.ObserveFields(owner).fields)
        if (row.error.empty() && row.declaration.name == name) {
            if (result || row.declaration.type != type || !row.declaration.flags_observed ||
                (row.declaration.flags & 16))
                throw std::runtime_error(std::string("BGM sync field differs: ") + name);
            result = row.field;
            if (offset)
                *offset = row.declaration.offset;
        }
    if (!result)
        throw std::runtime_error(std::string("BGM sync field absent: ") + name);
    return result;
}
}
BgmSyncBindings ResolveBgmSyncBindings(RuntimeMetadata& m, const BgmSyncType& type, const BgmSyncReference& reference) {
    BgmSyncBindings s;
    s.model_type = type("RPG.GameCore", "CharacterModelComponent").system;
    const auto component = type("", "CFFPEAALCPH");
    s.component_type = component.system;
    const auto helper = Field(m, component.kind, "LNKLEKJPNHK", "CFFPEAALCPH.JJGHFDADLPD", &s.helper_offset);
    Field(m, m.FieldType(helper), "<Config>k__BackingField", "RPG.GameCore.SyncAnimWithBgm", &s.config_offset);
    s.start = m.Resolve("", "CFFPEAALCPH", "AEIPMJBPHNN", "System.Void", {"RPG.GameCore.SyncAnimWithBgm"});
    s.stop = m.Resolve("", "CFFPEAALCPH", "LGIDEBOBDIE", "System.Void", {});
    s.animator = m.Resolve("", "CFFPEAALCPH", "GetUnityAnimator", "UnityEngine.Animator", {});
    s.controller = m.Resolve("UnityEngine", "Animator", "get_runtimeAnimatorController",
                             "UnityEngine.RuntimeAnimatorController", {});
    s.config_type = type("RPG.GameCore", "SyncAnimWithBgm");
    s.chapter_type = type("RPG.GameCore", "SyncAnimWithBgmChapterConfig");
    s.phase_type = type("RPG.GameCore", "SyncAnimWithBgmPhaseConfig");
    s.state_name = reference(s.config_type, "BGMStateName", "System.String");
    s.chapters = reference(s.config_type, "ChapterConfig", "RPG.GameCore.SyncAnimWithBgmChapterConfig[]");
    s.cue = reference(s.chapter_type, "ChapterCueEvent", "System.String");
    s.phases = reference(s.chapter_type, "PhaseConfig", "RPG.GameCore.SyncAnimWithBgmPhaseConfig[]");
    s.double_phases = reference(s.chapter_type, "x2PhaseConfig", "RPG.GameCore.SyncAnimWithBgmPhaseConfig[]");
    Field(m, s.config_type.kind, "BPM", "System.Single", &s.bpm_offset);
    Field(m, s.phase_type.kind, "StartTime", "System.Single", &s.time_offset);
    Field(m, s.phase_type.kind, "BlendParam", "System.Single", &s.blend_offset);
    Field(m, s.phase_type.kind, "NorTimeOffset", "System.Single", &s.normalized_offset);
    return s;
}
}
