#include "ContextBindings.hpp"
#include <stdexcept>

namespace hksr::api::detail {
ObjectResult ResolveActorComponentType(ManagedObjects& objects, ActorComponent component) {
    const char* name = nullptr;
    switch (component) {
    case ActorComponent::Model:
        name = "CharacterModelComponent";
        break;
    case ActorComponent::Visibility:
        name = "CharacterVisibleComponent";
        break;
    case ActorComponent::Transform:
        name = "TransformComponent";
        break;
    case ActorComponent::FreeStyle:
        name = "FreeStyleComponent";
        break;
    case ActorComponent::CharacterData:
        name = "CharacterDataComponent";
        break;
    }
    if (!name)
        throw std::invalid_argument("unknown actor component role");
    return objects.ManagedType("RPG.GameCore", name);
}
std::vector<DeclaredField> ReadMonoEffectFields(RuntimeMetadata& m) {
    return m.ReadFields("RPG.Client", "MonoEffect");
}
std::vector<DeclaredField> ReadTriggerEffectFields(RuntimeMetadata& m) {
    return m.ReadFields("RPG.GameCore", "TriggerEffect");
}
HitEffectControlMethods ResolveHitEffects(RuntimeMetadata& m) {
    return ResolveHitEffectControl(m, m.ReadFields("RPG.Client", "TriggerEffectParams"), ReadMonoEffectFields(m));
}
MovementGeometryAccess ResolveActorMovementGeometry(RuntimeMetadata& m) {
    return ResolveMovementGeometryAccess(m, m.ReadFields("RPG.GameCore", "CharacterDataComponent", false));
}
std::size_t ResolveStoryEventOffset(RuntimeMetadata& m) {
    std::size_t offset = 0;
    for (const auto& f : m.ReadFields("RPG.GameCore", "AdventureCharacterConfig", false))
        if (f.name == "AnimEventConfigListInPerformance" && f.type == "System.String[]") {
            if (offset)
                throw std::runtime_error("story event field is ambiguous");
            offset = f.offset;
        }
    return offset;
}
EffectFollowBindings ResolveEffectFollowBindings(RuntimeMetadata& m) {
    EffectFollowBindings out;
    try {
        const auto behavior = m.FindType("", "GIMGIMFGBFF");
        const auto observed = m.ObserveFields(behavior);
        RuntimeType list_type;
        for (const auto& field : observed.fields)
            if (field.declaration.name == "PJFPEKPDKIH") {
                if (!field.error.empty())
                    throw std::runtime_error(field.error);
                if (list_type)
                    throw std::runtime_error("follow list field is ambiguous");
                list_type = m.FieldType(field.field);
            }
        if (!list_type)
            throw std::runtime_error("follow list declaration unavailable: " + observed.iteration_error);
        out.methods = ResolveEffectEntityFollow(m);
        out.fields = ResolveEffectEntityFollowFields(ReadMonoEffectFields(m),
                                                     m.ReadFields("RPG.Client", "MonoEffectPluginEntityFollowEffect"),
                                                     m.ReadFields(behavior));
        out.list = ResolveReferenceListMethods(m.ReadType(list_type), "RPG.GameCore.GameEntity");
    } catch (const std::exception& e) {
        out.methods = {};
        out.methods.binding_errors.push_back(e.what());
    }
    // Motion inspection is independent of the follow plugin's binding contract.
    try {
        out.motion = ResolveEffectMotionTarget(m, m.ReadFields("", "BGOBCKLPEMK"));
    } catch (const std::exception& e) {
        out.methods.binding_errors.push_back(std::string("motion target: ") + e.what());
    }
    return out;
}
SelfPerformerRule HeadLookPerformerRule() {
    return {"RPG.GameCore.HeadLookAt", "PerformerType"};
}
}
