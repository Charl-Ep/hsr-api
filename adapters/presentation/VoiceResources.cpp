#include "../../src/presentation/audio/VoiceResources.hpp"
#include "../animation/catalog/CatalogSchema.hpp"
#include <map>
#include <memory>

namespace hksr::api::detail {
namespace {
struct Inputs : CatalogAccess {
    using CatalogAccess::CatalogAccess;
    std::map<std::string, std::int32_t> enums;
    std::map<std::uint32_t, std::string> tags;
    bool closed = false;
    std::int32_t Enum(const char* type, const char* name) {
        const auto key = std::string(type) + "." + name;
        const auto found = enums.find(key);
        if (found != enums.end())
            return found->second;
        Scope scope(*this);
        auto* enum_type = scope.Keep(objects.ManagedType("RPG.GameCore", type));
        auto* text = scope.Keep(objects.NewStringUtf8(name));
        auto* value =
            scope.Keep(Call(methods.Resolve("System", "Enum", "Parse", "System.Object",
                                            {"System.Type", "System.String", "System.Boolean"}, MethodKind::Static),
                            enum_type, text, false));
        const auto number = Read<std::int32_t>(value, FieldFor(value, "value__", "System.Int32"));
        enums.emplace(key, number);
        return number;
    }
    std::string Tag(std::uint32_t avatar) {
        const auto found = tags.find(avatar);
        if (found != tags.end())
            return found->second;
        if (!avatar)
            throw std::invalid_argument("voice resources require an Avatar ID");
        Scope scope(*this);
        auto* row = scope.Keep(Call(methods.Resolve("RPG.GameCore", "AvatarExcelTable", "GetData",
                                                    "RPG.GameCore.AvatarRow", {"System.UInt32"}, MethodKind::Static),
                                    avatar));
        const auto tag = Text(Read<void*>(row, FieldFor(row, "AvatarVOTag", "System.String")));
        tags.emplace(avatar, tag);
        return tag;
    }
    std::string Event(std::uint32_t avatar, VoiceCue cue) {
        if (closed)
            throw std::logic_error("voice resources closed");
        const char* name = nullptr;
        switch (cue) {
        case VoiceCue::LightHit:
            name = "LightHit";
            break;
        case VoiceCue::UltraReady:
            name = "UltraReady";
            break;
        }
        if (!name)
            throw std::invalid_argument("voice cue unsupported");
        const auto tag = Tag(avatar);
        if (tag.empty())
            return {};
        const auto kind = Enum("CharacterVOType", name);
        Scope scope(*this);
        auto* audio = scope.Keep(StaticValue(scope, "RPG.Client", "GlobalVars", "s_AudioManager"));
        auto* config = Read<void*>(audio, FieldFor(audio, "AudioConfig", "RPG.GameCore.AudioConfig"));
        Require(config != nullptr, "audio configuration absent");
        scope.Keep({{true, 0}, config});
        auto* map = Read<void*>(
            config, FieldFor(config, "CharacterVOEventMap",
                             "System.Collections.Generic.Dictionary<RPG.GameCore.CharacterVOType,System.String>"));
        Require(map != nullptr, "character voice templates absent");
        scope.Keep({{true, 0}, map});
        const auto value =
            Call(Instance(metadata.ReadObjectType(map), "get_Item", "System.String", {"RPG.GameCore.CharacterVOType"}),
                 map, kind);
        Require(value.call.returned, "character voice template read failed");
        if (!value.object || Text(value.object).empty())
            return {};
        auto* pattern = scope.Keep(value);
        auto* input = scope.Keep(objects.NewStringUtf8(tag.c_str()));
        return Text(scope.Keep(Call(methods.Resolve("System", "String", "Format", "System.String",
                                                    {"System.String", "System.Object"}, MethodKind::Static),
                                    pattern, input)));
    }
    std::optional<VoiceCue> Ready(std::int32_t type) {
        if (closed)
            throw std::logic_error("voice resources closed");
        return type == Enum("SkillType", "Ultra") ? std::optional<VoiceCue>(VoiceCue::UltraReady) : std::nullopt;
    }
    bool Exists(const std::string& event) {
        if (closed)
            throw std::logic_error("voice resources closed");
        Scope scope(*this);
        auto* audio = scope.Keep(StaticValue(scope, "RPG.Client", "GlobalVars", "s_AudioManager"));
        auto* text = scope.Keep(objects.NewStringUtf8(event.c_str()));
        bool exists = false;
        Require(native::Call(methods.Resolve("RPG.Client", "AudioManager", "IsEventExistInPck", "System.Boolean",
                                             {"System.String"}),
                             &exists, audio, text)
                    .returned,
                "voice event existence query failed");
        return exists;
    }
    bool Close() {
        closed = true;
        std::vector<ManagedRootId> batch;
        batch.swap(pending);
        for (auto id : batch)
            if (roots.Release(id).complete)
                roots.Forget(id);
            else
                pending.push_back(id);
        return pending.empty();
    }
};
}
VoiceResourceAccess ResolveVoiceResources(MethodResolver& methods, ObjectMetadata& metadata, ManagedObjects& objects,
                                          ReferenceMethods refs, ManagedArrays arrays, ComponentConfig text) {
    auto inputs = std::make_shared<Inputs>(methods, metadata, objects, refs, std::move(arrays), std::move(text));
    return {[inputs](std::uint32_t avatar, VoiceCue cue) { return inputs->Event(avatar, cue); },
            [inputs](const std::string& event) { return inputs->Exists(event); },
            [inputs](std::int32_t type) { return inputs->Ready(type); }, [inputs] { return inputs->Close(); }};
}
}
