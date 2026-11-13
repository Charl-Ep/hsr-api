#include "AnimationParameterBindings.hpp"
#include <stdexcept>
namespace hksr::api::detail {
namespace {
std::size_t Field(const std::vector<DeclaredField>& fields, const char* owner, const char* name, const char* type) {
    const DeclaredField* found = nullptr;
    for (const auto& f : fields)
        if (f.owner_namespace == "RPG.Client" && f.owner_name == owner && f.name == name) {
            if (found || f.type != type || !f.flags_observed || (f.flags & 0x10))
                throw std::runtime_error(std::string("animation propagation field differs: ") + owner + "." + name);
            found = &f;
        }
    if (!found)
        throw std::runtime_error(std::string("animation propagation field absent: ") + owner + "." + name);
    return found->offset;
}
}
void ResolveAnimationParameterBody(RuntimeMetadata& metadata, AnimationParameterBindings& s,
                                   const AnimationParameterRoot& root) {
    const auto type = metadata.ManagedType("RPG.GameCore", "CharacterModelComponent");
    if (!type.call.returned || !type.object)
        throw std::runtime_error("animation parameter model Type unavailable");
    root(type);
    s.model_type = type.object;
    s.mock = metadata.Resolve("RPG.GameCore", "GameEntityExtensions", "GetMockAnimator", "RPG.Client.MockAnimator",
                              {"RPG.GameCore.GameEntity"}, MethodKind::Static);
    s.mock_animator = metadata.Resolve("RPG.Client", "MockAnimator", "get_Animator", "UnityEngine.Animator", {});
    s.body = ResolveAnimationParameterSetters(metadata, "RPG.Client", "MockAnimator");
}
void ResolveAnimationParameterEffects(RuntimeMetadata& m, AnimationParameterBindings& s) {
    s.find_effects = m.Resolve("RPG.Client", "MonoEffectManager", "FindEffectsByEntity", "RPG.Client.MonoEffectList",
                               {"RPG.GameCore.GameEntity", "System.Boolean"});
    s.behavior = m.Resolve("RPG.Client", "MonoEffect", "get_Behavior", "RPG.Client.MonoEffectBehavior", {});
    s.animators =
        m.Resolve("RPG.Client", "MonoEffectBehavior", "GetAnimators", "RPG.PoolList<UnityEngine.Animator>", {});
    // Both returned collections may inherit their accessors from a
    // closed generic base. Keep that base's actual MethodInfo.
    auto effects = ResolveReferenceListHierarchy(m, m.ReturnType(s.find_effects), "RPG.Client.MonoEffect");
    auto children = ResolveReferenceListHierarchy(m, m.ReturnType(s.animators), "UnityEngine.Animator");
    if (!effects.count.code || !effects.item.code || !children.count.code || !children.item.code) {
        std::string error = "SyncPropState collection bindings unavailable";
        for (const auto& item : effects.binding_errors)
            error += "\nassociated effects: " + item;
        for (const auto& item : children.binding_errors)
            error += "\neffect Animators: " + item;
        throw std::runtime_error(error);
    }
    s.effect_list.emplace(effects);
    s.animator_list.emplace(children);
    const auto fields = m.ReadFields("RPG.Client", "TriggerEffectParams");
    s.relations.emplace(fields);
    s.request_offset = Field(m.ReadFields("RPG.Client", "MonoEffect"), "MonoEffect", "TriggerParams",
                             "RPG.Client.TriggerEffectParams");
    s.id_offset = Field(fields, "TriggerEffectParams", "<UniqueEffectID>k__BackingField", "System.UInt32");
    s.sync_offset = Field(fields, "TriggerEffectParams", "SyncPropState", "System.Boolean");
    s.effect = ResolveAnimationParameterSetters(m, "UnityEngine", "Animator");
}
}
