#include "VisualConfigurationBindings.hpp"
#include <stdexcept>

namespace hksr::api::detail {
namespace {
RuntimeTypeName TypeName(VisualConfigurationObject kind) {
    switch (kind) {
    case VisualConfigurationObject::Data:
        return {"RPG.GameCore", "CharacterDataComponent"};
    case VisualConfigurationObject::Emotion:
        return {"", VisualConfigurationTypes::emotion};
    case VisualConfigurationObject::EventComponent:
        return {"", "LGBHONLHNPN"};
    case VisualConfigurationObject::EventConsumer:
        return {"", VisualConfigurationTypes::events};
    case VisualConfigurationObject::Adaptation:
        return {"RPG.GameCore", "EffectAdaptionConfig"};
    case VisualConfigurationObject::Somato:
        return {"RPG.GameCore", "CharacterSomatoType"};
    }
    throw std::invalid_argument("unknown visual configuration object");
}
DeclaredField Select(const std::vector<DeclaredField>& fields, const char* name, const char* type) {
    const DeclaredField* found = nullptr;
    for (const auto& field : fields)
        if (field.name == name) {
            if (found || field.type != type || !field.flags_observed || (field.flags & 16))
                throw std::runtime_error("visual configuration declaration differs");
            found = &field;
        }
    if (!found)
        throw std::runtime_error("visual configuration field missing");
    return *found;
}
DeclaredField At(const std::vector<DeclaredField>& fields, std::size_t offset, const char* type) {
    const DeclaredField* found = nullptr;
    for (const auto& field : fields)
        if (field.offset == offset && field.type == type && field.flags_observed && !(field.flags & 16)) {
            if (found)
                throw std::runtime_error("visual overlay field ambiguous");
            found = &field;
        }
    if (!found)
        throw std::runtime_error("visual overlay layout differs");
    return *found;
}
}
ObjectResult ResolveVisualConfigurationObjectType(ManagedObjects& objects, VisualConfigurationObject kind) {
    const auto type = TypeName(kind);
    return objects.ManagedType(type.name_space.c_str(), type.name.c_str());
}
ObjectResult AllocateVisualConfigurationObject(ManagedObjects& objects, VisualConfigurationObject kind) {
    const auto type = TypeName(kind);
    return objects.AllocateObject(type.name_space.c_str(), type.name.c_str());
}
bool MatchesVisualConfigurationObject(ObjectMetadata& metadata, void* object, VisualConfigurationObject kind) {
    const auto expected = TypeName(kind), actual = metadata.ReadObjectTypeName(object);
    if (actual.name_space == expected.name_space && actual.name == expected.name)
        return true;
    if (auto* hierarchy = dynamic_cast<TypeRelationships*>(&metadata))
        return hierarchy->TypeDerivesFrom(actual.name_space.c_str(), actual.name.c_str(), expected.name_space.c_str(),
                                          expected.name.c_str());
    return false;
}
VisualConfigurationBindings ResolveVisualConfiguration(MethodResolver& methods,
                                                       const VisualConfigurationFields& fields) {
    VisualConfigurationBindings out;
    auto bind = [&](std::string& error, const char* name, auto prepare) {
        try {
            prepare();
        } catch (const std::exception& failure) {
            error = std::string("visual configuration ") + name + ": " + failure.what();
        }
    };
    bind(out.data_error, "data", [&] {
        const auto data = fields("RPG.GameCore", "CharacterDataComponent");
        out.overlay = Select(data, "_OverrideCharacterConfigParam", "IJDDNGPOEHK");
        out.data_owner = Select(data, "_OwnerRef", "RPG.GameCore.GameEntity").offset;
    });
    // Value-type offsets include the boxed object header. Each override and
    // its dependent native operations qualify independently.
    bind(out.radius_error, "radius", [&] {
        out.radius = At(fields("", "IJDDNGPOEHK"), 16 + 0x18, "System.Single");
        out.single_value = Select(fields("System", "Single"), "m_value", "System.Single").offset;
    });
    bind(out.somato_error, "somato", [&] {
        out.somato = At(fields("", "IJDDNGPOEHK"), 16 + 0x38, "RPG.GameCore.CharacterSomatoType");
        out.somato_value = Select(fields("RPG.GameCore", "CharacterSomatoType"), "value__", "System.Int32").offset;
    });
    bind(out.adaptations_error, "effect adaptations", [&] {
        out.adaptations = At(fields("", "IJDDNGPOEHK"), 16 + 0x40, "RPG.GameCore.EffectAdaptionConfig[]");
    });
    bind(out.adaptation_entries_error, "effect adaptations", [&] {
        const auto effect = fields("RPG.GameCore", "EffectAdaptionConfig");
        out.path = Select(effect, "EffectPath", "System.String");
        out.alias = Select(effect, "AliasName", "System.String");
        out.attach = Select(effect, "AttachPoint", "System.String");
        out.position = Select(effect, "PositionOffset", "RPG.MVector3").offset;
        out.rotation = Select(effect, "RotationOffset", "RPG.MVector3").offset;
        out.scale = Select(effect, "Scale", "System.Single").offset;
    });
    bind(out.adaptation_refresh_error, "effect adaptation refresh", [&] {
        out.adaptation_changed =
            methods.Resolve("RPG.GameCore", "CharacterDataComponent", "_OnEffectAdaptionListChanged", "System.Void",
                            {"RPG.GameCore.EffectAdaptionConfig[]", "RPG.GameCore.EffectAdaptionConfig[]"});
        out.get_adaptations = methods.Resolve("RPG.GameCore", "CharacterDataComponent", "get_EffectAdaptionList",
                                              "RPG.GameCore.EffectAdaptionConfig[]", {});
    });
    bind(out.graph_emotion_error, "graph emotion", [&] {
        out.graph_emotion = At(fields("", "IJDDNGPOEHK"), 16 + 0x48, "System.String");
    });
    bind(out.data_events_error, "data event files", [&] {
        out.event_files = At(fields("", "IJDDNGPOEHK"), 16 + 0x28, "System.String[]");
    });
    bind(out.events_read_error, "effective event files", [&] {
        out.get_events =
            methods.Resolve("RPG.GameCore", "CharacterDataComponent", "get_AnimEventConfigList", "System.String[]", {});
    });
    bind(out.consumer_error, "event consumer", [&] {
        out.bind_events =
            methods.Resolve("", "HHECHLOIEJO", "FOGNIIEDKCH", "System.Void", {"System.String[]", "System.String[]"});
        const auto consumer = fields("", "HHECHLOIEJO");
        out.consumer_paths = Select(consumer, "KGGGDHNMDMK", "System.String[]").offset;
        out.consumer_layers = Select(consumer, "NBNJJDIMDKE", "System.String[]").offset;
        out.consumer_ready = Select(consumer, "CPIIOIPAAKK", "IBEJMEEEJMB").offset;
    });
    bind(out.emotion_error, "emotion", [&] {
        out.emotion_owner = Select(fields("", "PEPNFLNCDHG"), "_OwnerRef", "RPG.GameCore.GameEntity").offset;
        out.emotion_enable = methods.Resolve("", "PEPNFLNCDHG", "EMFIGLCNJIP", "System.Void",
                                             {"PEPNFLNCDHG.EmoEnableSource", "System.Boolean", "System.Single"});
        out.emotion_blendshape = methods.Resolve("", "PEPNFLNCDHG", "HNFHGGINBHF", "System.Void", {"System.Boolean"});
    });
    return out;
}
}
