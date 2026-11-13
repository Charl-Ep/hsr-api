#include "TimelineBindings.hpp"
#include <hksr/native/MetadataReader.hpp>
#include <stdexcept>

namespace hksr::api::detail {
namespace {
void CheckField(const std::vector<DeclaredField>& fields, const char* owner, const char* name, const char* type,
                std::size_t offset) {
    unsigned matches = 0;
    for (const auto& f : fields)
        if (f.owner_namespace == "UnityEngine.Playables" && f.owner_name == owner && f.name == name) {
            ++matches;
            if (f.type != type || f.offset != offset || !f.flags_observed || (f.flags & 0x10))
                throw std::runtime_error(std::string("Playable value ABI differs: ") + owner + "." + name);
        }
    if (matches != 1)
        throw std::runtime_error(std::string("Playable value field absent/ambiguous: ") + owner + "." + name);
}
}
TimelineGraphBindings ResolveTimelineGraphBindings(MethodResolver& m, const TimelineRegistryAccess::Fields& fields) {
    CheckField(fields("UnityEngine.Playables", "Playable"), "Playable", "m_Handle",
               "UnityEngine.Playables.PlayableHandle", 16);
    for (const auto* name : {"PlayableHandle", "PlayableGraph"}) {
        const auto f = fields("UnityEngine.Playables", name);
        CheckField(f, name, "m_Handle", "System.IntPtr", 16);
        CheckField(f, name, "m_Version", "System.UInt32", 24);
    }
    return {m.Resolve("UnityEngine.Playables", "PlayableHandle", "GetGraph_Injected", "System.Void",
                      {"UnityEngine.Playables.PlayableHandle&", "UnityEngine.Playables.PlayableGraph&"},
                      MethodKind::Static),
            m.Resolve("UnityEngine.Playables", "PlayableGraph", "IsValid_Injected", "System.Boolean",
                      {"UnityEngine.Playables.PlayableGraph&"}, MethodKind::Static),
            m.Resolve("UnityEngine.Playables", "PlayableGraph", "GetResolver_Injected",
                      "UnityEngine.IExposedPropertyTable", {"UnityEngine.Playables.PlayableGraph&"},
                      MethodKind::Static)};
}
bool IsTimelineDirector(const DeclaredType& type) {
    return type.name_space == "UnityEngine.Playables" && type.name == "PlayableDirector";
}
bool ReadPlayableToken(const void* input, PlayableToken& out) {
    return input && native::CopyMemoryChecked(&out, input, sizeof(out));
}
TimelineAssetBindings ResolveTimelineAssetBindings(MethodResolver& m) {
    return {m.Resolve("UnityEngine.Timeline", "TimelineAsset", "get_flattenedTracks",
                      "System.Collections.Generic.IEnumerable<UnityEngine.Timeline.TrackAsset>", {}),
            m.Resolve("UnityEngine.Timeline", "TrackAsset", "get_mutedInHierarchy", "System.Boolean", {}),
            m.Resolve("UnityEngine.Timeline", "TrackAsset", "GetMarkerCount", "System.Int32", {})};
}
ObjectResult ResolveTimelineAssetType(ManagedObjects& o) {
    return o.ManagedType("UnityEngine.Timeline", "TimelineAsset");
}
bool IsTimelineAsset(const DeclaredType& type) {
    return type.name_space == "UnityEngine.Timeline" && type.name == "TimelineAsset";
}
ReferenceListMethods ResolveTimelineTrackList(const DeclaredType& type) {
    return ResolveReferenceListMethods(type, "UnityEngine.Timeline.TrackAsset");
}
std::array<Method, 3> ResolveTimelineEnvironmentEntries(MethodResolver& m, TimelineEnvironmentKind kind) {
    const bool adjust = kind == TimelineEnvironmentKind::AdjustLevel;
    const auto* ns = adjust ? "RPGTools.Timeline.EnvironmentSyetem" : "RPGTools.Timeline";
    const auto* type = kind == TimelineEnvironmentKind::CharacterLight ? "LightControlBehaviour"
                       : kind == TimelineEnvironmentKind::SceneLight   ? "SceneLightControlBehaviour"
                                                                       : "EnviroSysAdjustLevelBehaviour";
    std::array<Method, 3> result;
    for (std::size_t phase = 0; phase < 3; ++phase) {
        const auto* name = phase == 1 ? "ProcessFrame"
                           : adjust   ? (phase == 0 ? "OnGraphStart" : "OnGraphStop")
                                      : (phase == 0 ? "OnBehaviourPlay" : "OnBehaviourPause");
        result[phase] =
            phase == 1
                ? m.Resolve(ns, type, name, "System.Void",
                            {"UnityEngine.Playables.Playable", "UnityEngine.Playables.FrameData", "System.Object"})
            : adjust ? m.Resolve(ns, type, name, "System.Void", {"UnityEngine.Playables.Playable"})
                     : m.Resolve(ns, type, name, "System.Void",
                                 {"UnityEngine.Playables.Playable", "UnityEngine.Playables.FrameData"});
    }
    return result;
}
}
