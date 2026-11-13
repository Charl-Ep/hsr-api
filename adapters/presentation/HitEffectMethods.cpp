#include <hksr/presentation/effects/HitEffectControl.hpp>
#include <stdexcept>

namespace hksr::api {
namespace {
std::size_t Field(const std::vector<DeclaredField>& fields, const char* owner, const char* name, const char* type) {
    const DeclaredField* found = nullptr;
    for (const auto& f : fields)
        if (f.owner_namespace == "RPG.Client" && f.owner_name == owner && f.name == name) {
            if (found || f.type != type || !f.flags_observed || (f.flags & 0x10))
                throw std::runtime_error(std::string("hit effect field differs: ") + name);
            found = &f;
        }
    if (!found)
        throw std::runtime_error(std::string("hit effect field absent: ") + name);
    return found->offset;
}
}
HitEffectControlMethods ResolveHitEffectControl(MethodResolver& resolver, const std::vector<DeclaredField>& parameters,
                                                const std::vector<DeclaredField>& effects) {
    HitEffectControlMethods out;
    out.align = resolver.Resolve("", "PGOOHIHKHNJ", "DMFKGEBBGGG", "System.Void",
                                 {"RPG.Client.MonoEffect", "RPG.MVector3"}, MethodKind::Static);
    out.refresh_attachment =
        resolver.Resolve("RPG.Client", "MonoEffectManager", "_TryGetAttachPointFromEntityAfterLoadEffect",
                         "System.Void", {"RPG.Client.TriggerEffectParams", "RPG.Client.MonoEffect"});
    out.replay = resolver.Resolve("RPG.Client", "MonoEffectBehavior", "Replay", "System.Void", {"System.Boolean"});
    out.revive = resolver.Resolve("RPG.Client", "MonoEffectBehavior", "Revive", "System.Void", {});
    out.mutex_entity =
        resolver.Resolve("RPG.GameCore", "GamePlayStatic", "IsTriggerEffectMutexEntity", "RPG.Client.EffectMutexResult",
                         {"RPG.Client.MonoEffectManager", "System.String", "System.Single", "System.Int32",
                          "RPG.GameCore.GameEntity", "System.Single"},
                         MethodKind::Static);
    out.mutex_global = resolver.Resolve(
        "RPG.GameCore", "GamePlayStatic", "IsTriggerEffectMutexGlobal", "RPG.Client.EffectMutexResult",
        {"RPG.Client.MonoEffectManager", "System.String", "System.Single", "System.Int32"}, MethodKind::Static);
    out.first_active = resolver.Resolve("RPG.Client", "MonoEffectManager", "GetFirstActiveEffect",
                                        "RPG.Client.MonoEffect", {"System.String", "RPG.GameCore.GameEntity"});
    out.first_fading = resolver.Resolve("RPG.Client", "MonoEffectManager", "GetFirstFadeOutEffect",
                                        "RPG.Client.MonoEffect", {"System.String", "RPG.GameCore.GameEntity"});
    out.request_offset = Field(effects, "MonoEffect", "TriggerParams", "RPG.Client.TriggerEffectParams");
    out.behavior_offset = Field(effects, "MonoEffect", "_Behavior", "RPG.Client.MonoEffectBehavior");
    out.effect_delay_offset = Field(effects, "MonoEffect", "Delay", "System.Single");
    out.position_offset =
        Field(parameters, "TriggerEffectParams", "PositionOffset", "System.Nullable<UnityEngine.Vector3>");
    out.ignore_attach_offset = Field(parameters, "TriggerEffectParams", "IgnorePrefabAttachPoint", "System.Boolean");
    out.follow_first_frame_offset =
        Field(parameters, "TriggerEffectParams", "FollowFirstFrameOnLateTick", "System.Boolean");
    out.delay_offset = Field(parameters, "TriggerEffectParams", "Delay", "System.Single");
    out.enable_replay_offset = Field(effects, "MonoEffect", "EnableReplay", "System.Boolean");
    out.request_replay_offset = Field(parameters, "TriggerEffectParams", "IsEnableReplay", "System.Boolean");
    out.path_equal = resolver.Resolve("System", "String", "op_Equality", "System.Boolean",
                                      {"System.String", "System.String"}, MethodKind::Static);
    out.path_offset = Field(effects, "MonoEffect", "<EffectName>k__BackingField", "System.String");
    // Unity's inherited native-object field retains its actual declaring type.
    for (const auto& f : effects)
        if (f.owner_namespace == "UnityEngine" && f.owner_name == "Object" && f.name == "m_CachedPtr" &&
            f.type == "System.IntPtr" && f.flags_observed && !(f.flags & 0x10))
            out.native_pointer_offset = f.offset;
    if (!out.native_pointer_offset)
        throw std::runtime_error("hit effect native identity field absent");
    out.fields_available = true;
    return out;
}
}
