#include "../../src/model/CharacterSkinNative.hpp"
#include "../animation/catalog/CatalogSchema.hpp"
#include "../animation/catalog/CatalogDisplay.hpp"
#include <algorithm>
#include <set>

namespace hksr::api::detail {
namespace {
bool ReleaseSkinRoots(ManagedRoots& roots, std::vector<ManagedRootId>& receipts) {
    // Native release may re-enter the owner. Never hold a vector iterator or
    // reference across that call, and retain every unresolved receipt.
    std::vector<ManagedRootId> batch;
    batch.swap(receipts);
    for (auto id : batch) {
        if (!id)
            continue;
        if (roots.Release(id).complete)
            roots.Forget(id);
        else
            receipts.push_back(id);
    }
    return receipts.empty();
}
struct SkinInputs : CatalogSchema {
    using CatalogSchema::CatalogSchema;
    void* skin_data = nullptr;
    Method map_resource, skin_sound_exists;
    std::string audio_tag;
    std::set<std::string> selected_sounds;
    std::vector<ManagedRootId> held;
    bool closed = false, preview_unresolved = false, closing = false;
    std::size_t operations = 0;
    struct Operation {
        SkinInputs& owner;
        explicit Operation(SkinInputs& value) : owner(value) {
            ++owner.operations;
        }
        ~Operation() {
            --owner.operations;
        }
    };
    void* Hold(ObjectResult result) {
        Require(result.call.returned && result.object, "skin input unavailable");
        const auto index = held.size();
        held.emplace_back(0);
        const auto id = roots.Keep(result.object);
        held[index] = id;
        Require(roots.Status(id).ready, "skin input retention failed");
        return result.object;
    }
    Method Client(const char* type, const char* name, const char* result,
                  std::initializer_list<const char*> args = {}) {
        return methods.Resolve("RPG.Client", type, name, result, args, MethodKind::Static);
    }
    Method GetterFor(void* object, const char* name, const char* result) {
        const auto type = metadata.ReadObjectType(object);
        return methods.Resolve(type.name_space.c_str(), type.name.c_str(), name, result, {}, MethodKind::Instance);
    }
    std::string GetString(void* object, const char* name, Scope& scope) {
        const auto value = Call(GetterFor(object, name, "System.String"), object);
        Require(value.call.returned, "skin string getter failed");
        return value.object ? Text(scope.Keep(value)) : std::string{};
    }
    std::optional<std::string> Map(const std::string& path) {
        if (closed)
            throw std::logic_error("skin inputs closed");
        Operation operation(*this);
        Scope scope(*this);
        auto input = scope.Keep(objects.NewStringUtf8(path.c_str()));
        const auto value = Call(map_resource, skin_data, input);
        Require(value.call.returned, "official skin resource mapping failed");
        if (!value.object)
            return {};
        return Text(scope.Keep(value));
    }
    std::string Sound(const std::string& event) {
        if (closed)
            throw std::logic_error("skin inputs closed");
        Operation operation(*this);
        if (audio_tag.empty() || selected_sounds.count(event))
            return event;
        if (!skin_sound_exists.code)
            skin_sound_exists =
                methods.Resolve("RPG.Client", "AudioManager", "IsEventExistInPck", "System.Boolean", {"System.String"});
        Scope scope(*this);
        const auto candidate = event + "_" + audio_tag; // TriggerSoundInAnim's {0}_{1} selection.
        auto* manager = scope.Keep(StaticValue(scope, "RPG.Client", "GlobalVars", "s_AudioManager"));
        auto* input = scope.Keep(objects.NewStringUtf8(candidate.c_str()));
        bool exists = false;
        Require(native::Call(skin_sound_exists, &exists, manager, input).returned, "skin sound existence query failed");
        if (exists)
            selected_sounds.insert(candidate);
        return exists ? candidate : event;
    }
    std::vector<std::string> Events(void* config, Scope& scope) {
        const auto values = Strings(config, metadata.ReadObjectFields(config), "AnimEventConfigList");
        Require(values.available, "skin animation event list unreadable");
        std::vector<std::string> out;
        for (const auto& path : values.values)
            if (auto mapped = Map(path))
                out.push_back(std::move(*mapped));
        (void)scope;
        return out;
    }
    CharacterSkinStatus Prepare(std::uint32_t id) {
        Operation operation(*this);
        CharacterSkinStatus out;
        out.skin_id = id;
        Scope scope(*this);
        void* preview = nullptr;
        bool dispose_required = false;
        Method dispose;
        try {
            Require(id != 0, "skin ID must be nonzero");
            // Resolve the exact cleanup before obtaining an owned preview.
            dispose = methods.Resolve("RPG.Client", "PreviewAvatarProxy", "Dispose", "System.Void", {});
            auto* row = scope.Keep(
                Call(Static("AvatarSkinExcelTable", "GetData", "RPG.GameCore.AvatarSkinRow", {"System.UInt32"}), id));
            out.avatar_id = Read<std::uint32_t>(row, FieldFor(row, "AvatarID", "System.UInt32"));
            Require(Read<std::uint32_t>(row, FieldFor(row, "ID", "System.UInt32")) == id, "skin row identity differs");
            auto result =
                Call(Client("AvatarHelper", "GetSkinPreviewAvatar", "RPG.AvatarSystem.IAvatar", {"System.UInt32"}), id);
            Require(result.call.returned && result.object, "skin preview unavailable");
            preview = result.object;
            const auto type = metadata.ReadObjectTypeName(preview);
            auto* relationships = dynamic_cast<TypeRelationships*>(&metadata);
            const bool proxy = type.name_space == "RPG.Client" && type.name == "PreviewAvatarProxy";
            Require(proxy ||
                        (relationships && relationships->TypeDerivesFrom(type.name_space.c_str(), type.name.c_str(),
                                                                         "RPG.Client", "PreviewAvatarProxy")),
                    "skin preview ownership type differs");
            dispose_required = true;
            scope.Keep(result);
            std::uint32_t actual = 0;
            Require(native::Call(
                        Client("AvatarExtensions", "GetDressedSkinID", "System.UInt32", {"RPG.AvatarSystem.IAvatar"}),
                        &actual, preview)
                            .returned &&
                        actual == id,
                    "skin preview selection differs");
            skin_data = Hold(Call(Client("AvatarSkinDataFactory", "CreateByAvatarSkinId", "RPG.Client.IAvatarSkinData",
                                         {"System.UInt32"}),
                                  id));
            std::uint32_t avatar = 0;
            Require(native::Call(GetterFor(skin_data, "get_AvatarId", "System.UInt32"), &avatar, skin_data).returned &&
                        avatar == out.avatar_id,
                    "skin owner differs");
            const auto data_type = metadata.ReadObjectTypeName(skin_data);
            map_resource = methods.Resolve(data_type.name_space.c_str(), data_type.name.c_str(), "MapToSkinRes",
                                           "System.String", {"System.String"});
            audio_tag = out.audio_tag = GetString(skin_data, "get_AudioEventTag", scope);
            out.adventure_config = Text(scope.Keep(Call(
                Client("AvatarExtensions", "GetAdventureJsonConfigPath", "System.String", {"RPG.AvatarSystem.IAvatar"}),
                preview)));
            auto* effective =
                scope.Keep(Call(Client("AvatarExtensions", "GetModifiedAdventureCharacterConfig",
                                       "RPG.GameCore.AdventureCharacterConfig", {"RPG.AvatarSystem.IAvatar"}),
                                preview));
            out.adventure_event_files = Events(effective, scope);
            auto* avatar_row = scope.Keep(Call(
                Static("AvatarExcelTable", "GetData", "RPG.GameCore.AvatarRow", {"System.UInt32"}), out.avatar_id));
            out.battle_config = String(avatar_row, metadata.ReadObjectFields(avatar_row), "JsonPath");
            auto* battle = Config("CharacterConfig", out.battle_config, scope);
            const auto override_path = GetString(skin_data, "get_CharacterSkinOverrideConfigPath", scope);
            if (!override_path.empty()) {
                const auto clone = methods.Resolve("System", "Object", "MemberwiseClone", "System.Object", {});
                auto* copy = scope.Keep(Call(clone, battle));
                Require(copy != battle, "skin battle configuration clone aliases shared source");
                auto* skin_config = Config("SkinCharacterOverrideConfig", override_path, scope);
                const auto apply = methods.Resolve("RPG.GameCore", "SkinCharacterOverrideConfig", "PKADDEPKJFN",
                                                   "System.Void", {"RPG.GameCore.CharacterConfig"});
                Require(native::CallVoid(apply, skin_config, copy).returned,
                        "official skin battle configuration override failed");
                battle = copy;
            }
            out.battle_event_files = Events(battle, scope);
            const auto adventure = Map(GetString(skin_data, "get_AdvPlayerPrefabPath", scope));
            const auto model = Map(GetString(skin_data, "get_DefaultAvatarModelPath", scope));
            Require(adventure && model && !adventure->empty() && !model->empty(),
                    "skin model was suppressed or unavailable");
            out.adventure_model = *adventure;
            out.battle_model = *model;
            out.prepared = true;
        } catch (const std::exception& error) {
            out.errors.push_back(error.what());
        }
        if (dispose_required) {
            const auto result = native::CallVoid(dispose, preview);
            if (!result.returned) {
                preview_unresolved = true;
                out.prepared = false;
                out.errors.push_back("skin preview Dispose failed");
                // Keep the exact unresolved owned proxy; GC release is not pool return.
                try {
                    Hold({{true, 0}, preview});
                } catch (const std::exception& e) {
                    out.errors.push_back(e.what());
                }
            }
        }
        return out;
    }
    bool Close() {
        if (preview_unresolved || operations || closing)
            return false; // no blind second Dispose or in-flight input release
        closed = true;
        closing = true;
        struct Exit {
            bool& value;
            ~Exit() {
                value = false;
            }
        } exit{closing};
        const auto owned = ReleaseSkinRoots(roots, held);
        const auto temporary = ReleaseSkinRoots(roots, pending);
        return owned && temporary;
    }
};
}
CharacterSkinCatalog CharacterSkinFactory::Read(MethodResolver& methods, ObjectMetadata& metadata,
                                                ManagedObjects& objects, ReferenceMethods references,
                                                ManagedArrays arrays, ComponentConfig text,
                                                std::function<bool()>& cleanup) {
    auto retained =
        std::make_shared<CatalogSchema>(methods, metadata, objects, references, std::move(arrays), std::move(text));
    auto& access = *retained;
    cleanup = [retained] { return ReleaseSkinRoots(retained->roots, retained->pending); };
    CatalogBatch<CharacterSkinRow> rows;
    CatalogDisplay display(access);
    std::vector<std::string> display_errors;
    access.Table(rows, "AvatarSkinExcelTable", "RPG.GameCore.AvatarSkinRow", [&](void* row, const auto& fields) {
        CharacterSkinRow out;
        out.skin_id = catalog_detail::Read<std::uint32_t>(row, Field(fields, "ID", "System.UInt32"));
        out.avatar_id = catalog_detail::Read<std::uint32_t>(row, Field(fields, "AvatarID", "System.UInt32"));
        try {
            out.name = display.SkinName(out.skin_id);
        } catch (const std::exception& error) {
            display_errors.push_back(std::string("skin display name: ") + error.what());
        }
        return out;
    });
    CharacterSkinCatalog out;
    out.rows = std::move(rows.rows);
    out.complete = rows.enumerated && rows.issues.empty() && access.pending.empty();
    for (const auto& issue : rows.issues)
        out.errors.push_back(issue.operation + ": " + issue.error);
    out.errors.insert(out.errors.end(), display_errors.begin(), display_errors.end());
    if (!access.pending.empty())
        out.errors.push_back("skin catalog temporary reference release unresolved");
    std::sort(out.rows.begin(), out.rows.end(), [](const auto& a, const auto& b) { return a.skin_id < b.skin_id; });
    return out;
}
std::shared_ptr<CharacterSkin> CharacterSkinFactory::Prepare(MethodResolver& methods, ObjectMetadata& metadata,
                                                             ManagedObjects& objects, ReferenceMethods references,
                                                             ManagedArrays arrays, ComponentConfig text,
                                                             std::uint32_t id) {
    auto inputs =
        std::make_shared<SkinInputs>(methods, metadata, objects, references, std::move(arrays), std::move(text));
    auto status = inputs->Prepare(id);
    return Create(
        std::move(status), [inputs](const auto& path) { return inputs->Map(path); },
        [inputs](const auto& event) { return inputs->Sound(event); }, [inputs] { return inputs->Close(); });
}
}
