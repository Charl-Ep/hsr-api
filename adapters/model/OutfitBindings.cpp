#include "OutfitBindings.hpp"
#include <hksr/native/MetadataReader.hpp>
#include <hksr/native/Invocation.hpp>
#include <stdexcept>

namespace hksr::api::detail {
namespace {
struct OutfitSources {
    Method gender, player, path, models;
};
template <class T> T Field(ObjectMetadata& metadata, void* object, const char* name, const char* type) {
    unsigned matches = 0;
    std::size_t offset = 0;
    for (const auto& field : metadata.ReadObjectFields(object))
        if (field.name == name && field.type == type && field.flags_observed && !(field.flags & 16)) {
            ++matches;
            offset = field.offset;
        }
    T value{};
    if (matches != 1 || !native::CopyMemoryChecked(&value, static_cast<unsigned char*>(object) + offset, sizeof(value)))
        throw std::runtime_error(std::string("outfit field unavailable: ") + name);
    return value;
}
}
Method ResolveOutfitApplicability(MethodResolver& m) {
    return m.Resolve("RPG.GameCore", "CharacterOutfitComponent", "IsPlayerAvatarID", "System.Boolean",
                     {"System.UInt32"}, MethodKind::Static);
}
namespace {
OutfitSources ResolveOutfitSources(MethodResolver& m) {
    return {m.Resolve("RPG.Client", "AvatarHelper", "GetGender", "RPG.GameCore.GenderType", {"System.UInt32"},
                      MethodKind::Static),
            m.Resolve("RPG.Client", "PlayerOutfitHelper", "GetPlayerOutfit", "RPG.Client.AvatarOutfit", {},
                      MethodKind::Static),
            m.Resolve("RPG.Client", "AvatarHelper", "GetAvatarPath", "RPG.AvatarSystem.AvatarPath", {"System.UInt32"},
                      MethodKind::Static),
            m.Resolve("RPG.Client", "AvatarOutfit", "GetModels", "System.Collections.Generic.List<FJEOAPBLMKG>",
                      {"RPG.GameCore.GenderType", "RPG.AvatarSystem.AvatarPath"})};
}
}
OutfitSelection SelectOutfitModels(MethodResolver& methods, ObjectMetadata& metadata, std::uint32_t avatar,
                                   const std::function<void*(ObjectResult)>& retain) {
    const auto bindings = ResolveOutfitSources(methods);
    OutfitSelection selected;
    if (!native::Call(bindings.gender, &selected.gender, avatar).returned)
        throw std::runtime_error("official Avatar gender read failed");
    const auto keep = [&](ObjectResult value) {
        if (!value.call.returned || !value.object)
            throw std::runtime_error("outfit object unavailable");
        return retain(value);
    };
    try {
        const auto row = methods.Resolve("RPG.GameCore", "AvatarExcelTable", "GetData", "RPG.GameCore.AvatarRow",
                                         {"System.UInt32"}, MethodKind::Static);
        ObjectResult source;
        source.call = native::Call(row, &source.object, avatar);
        selected.base_type = Field<int>(metadata, keep(source), "AvatarBaseType", "RPG.GameCore.AvatarBaseType");
    } catch (const std::exception& error) {
        selected.errors.push_back(std::string("outfit base-type diagnostic unavailable: ") + error.what());
    }
    ObjectResult account;
    account.call = native::Call(bindings.player, &account.object);
    auto* outfit = keep(account);
    ObjectResult path;
    path.call = native::Call(bindings.path, &path.object, avatar);
    if (!path.call.returned) {
        selected.models = path;
        return selected;
    }
    // AvatarPath is an official reference object, not an enum representation.
    // The caller owns the root; null is a valid mapping result and is forwarded.
    auto* selected_path = retain(path);
    selected.models.call =
        native::Call(bindings.models, &selected.models.object, outfit, selected.gender, selected_path);
    return selected;
}
Method ResolveOutfitReplace(MethodResolver& m) {
    return m.Resolve("RPG.GameCore", "CharacterOutfitComponent", "OverrideOutfit", "System.Void",
                     {"System.Collections.Generic.List<FJEOAPBLMKG>", "System.Boolean"});
}
Method ResolveOutfitSourceModels(MethodResolver& m) {
    return m.Resolve("RPG.GameCore", "CharacterOutfitComponent", "LoadSourceOutfitModels",
                     "System.Collections.Generic.List<FJEOAPBLMKG>", {});
}
Method ResolveOutfitRefresh(MethodResolver& m) {
    return m.Resolve("RPG.GameCore", "CharacterOutfitComponent", "RefreshOutfitParts", "System.Void", {});
}
ObjectResult ResolveOutfitComponentType(ManagedObjects& objects) {
    return objects.ManagedType("RPG.GameCore", "CharacterOutfitComponent");
}
ReferenceListMethods ResolveOutfitModelList(ObjectMetadata& metadata, void* list) {
    return ResolveReferenceListMethods(metadata.ReadObjectType(list), "FJEOAPBLMKG");
}
void* ReadOutfitOverride(ObjectMetadata& metadata, void* object) {
    return Field<void*>(metadata, object, "_OverrideOutfitModels", "System.Collections.Generic.List<FJEOAPBLMKG>");
}
OutfitModel ReadOutfitModel(ObjectMetadata& metadata, void* object) {
    return {Field<std::uint32_t>(metadata, object, "<ID>k__BackingField", "System.UInt32"),
            Field<void*>(metadata, object, "<Config>k__BackingField", "RPG.GameCore.AvatarOutfitModelConfig")};
}
void* ReadOutfitConfigParts(ObjectMetadata& metadata, void* object, bool decorations) {
    return Field<void*>(metadata, object, decorations ? "DecorationConfigList" : "PartConfigList",
                        decorations ? "RPG.GameCore.AvatarOutfitDecorationConfig[]"
                                    : "RPG.GameCore.AvatarOutfitPartConfig[]");
}
void* ReadOutfitString(ObjectMetadata& metadata, void* object, OutfitString field) {
    const auto* name = field == OutfitString::Prefab     ? "PrefabConfigPath"
                       : field == OutfitString::Material ? "MaterialKey"
                                                         : "MeshConfigPath";
    return Field<void*>(metadata, object, name, "System.String");
}
bool ReadOutfitStatic(ObjectMetadata& metadata, void* object) {
    return Field<bool>(metadata, object, "IsStatic", "System.Boolean");
}
}
