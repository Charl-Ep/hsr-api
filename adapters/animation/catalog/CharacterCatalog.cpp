#include "CatalogSchema.hpp"
#include "CatalogDisplay.hpp"

namespace hksr::api::detail {
TextInput CatalogSchema::ReadTextLanguage() {
    TextInput output;
    Scope scope(*this);
    try {
        output.value = Text(scope.Keep(Call(Static("TextmapExcelTable", "GetLanguage", "System.String"))));
        Require(!output.value.empty(), "official text language not ready");
        output.available = true;
    } catch (const std::exception& error) {
        output.error = error.what();
    }
    return output;
}
CharacterDisplayName CatalogSchema::ReadCharacterDisplayName(CharacterNameKind kind, std::uint32_t id) {
    CharacterDisplayName output;
    CatalogDisplay display(*this);
    auto read = [](TextInput& field, const auto& body) {
        try {
            field.value = body();
            field.available = true;
        } catch (const std::exception& error) {
            field.error = error.what();
        }
    };
    switch (kind) {
    case CharacterNameKind::Npc:
        read(output.name, [&] { return display.NpcName(id); });
        break;
    case CharacterNameKind::Player:
        read(output.name, [&] { return display.Name(id); });
        read(output.path_name, [&] { return display.PathName(id); });
        read(output.default_skin_name, [&] { return display.DefaultSkinName(id); });
        break;
    case CharacterNameKind::Skin:
        read(output.name, [&] { return display.SkinName(id); });
        break;
    default:
        output.name.error = "unknown character display identity kind";
        break;
    }
    return output;
}
CatalogBatch<PlayerCatalogRow> CatalogSchema::Players() {
    CatalogBatch<PlayerCatalogRow> output;
    Scope scope(*this);
    try {
        auto list = scope.Keep(Call(
            Static("AdventurePlayerCommonWrap", "GetAllWrapData",
                   "System.Collections.Generic.List<RPG.GameCore.IAdventurePlayerCommonRowWrap>", {"System.Boolean"}),
            false));
        auto access = ManagedReferenceList(
            ResolveReferenceListMethods(metadata.ReadObjectType(list), "RPG.GameCore.IAdventurePlayerCommonRowWrap"));
        auto count = access.Count(list);
        Require(count.read.returned && count.value >= 0, "player wrapper count unavailable");
        output.declared_count = count.value;
        for (int i = 0; i < count.value; ++i) {
            ++output.visited_count;
            try {
                auto item = access.Item(list, i);
                Require(item.call.returned && item.object, "player wrapper missing");
                const auto type = metadata.ReadObjectType(item.object);
                PlayerCatalogRow row;
                auto number = [&](const char* name, std::uint32_t& value) {
                    Require(native::Call(Instance(type, name, "System.UInt32"), &value, item.object).returned,
                            "wrapper ID getter failed");
                };
                number("get_ID", row.id);
                number("get_AvatarID", row.avatar);
                number("get_EnhancedID", row.enhanced);
                number("get_DevSkinID", row.skin);
                auto string = [&](const char* name) {
                    auto value = Call(Instance(type, name, "System.String"), item.object);
                    Require(value.call.returned, "wrapper path getter failed");
                    Scope string_scope(*this);
                    if (value.object)
                        string_scope.Keep(value);
                    return Text(value.object);
                };
                row.character_config = string("get_PlayerJsonPath");
                row.prefab = string("get_PlayerPrefabPath");
                output.rows.push_back(std::move(row));
            } catch (const std::exception& error) {
                output.issues.push_back({i, "player wrapper", error.what()});
            }
        }
        output.enumerated = true;
    } catch (const std::exception& error) {
        output.issues.push_back({-1, "GetAllWrapData", error.what()});
    }
    return output;
}
CharacterCatalog CatalogSchema::ReadCharacters() {
    CharacterCatalog output;
    CatalogDisplay display(*this);
    this->Table(output.npcs, "NPCDataExcelTable", "RPG.GameCore.NPCDataRow", [&](void* row, const auto& fields) {
        NpcCatalogRow out{Read<std::uint32_t>(row, Field(fields, "ID", "System.UInt32")),
                          this->String(row, fields, "JsonPath"), this->String(row, fields, "ConfigEntityPath")};
        if (std::any_of(fields.begin(), fields.end(), [](const auto& field) {
                return field.name == "DefaultNPCName" && field.type == "RPG.Client.TextID";
            })) {
            try {
                out.name = display.Localize(row, fields, "DefaultNPCName");
            } catch (const std::exception& error) {
                output.npcs.issues.push_back({-1, "NPC display name", error.what()});
            }
        }
        return out;
    });
    output.players = this->Players();
    std::map<std::uint32_t, std::string> initial_skins;
    this->Table(output.avatars, "AvatarExcelTable", "RPG.GameCore.AvatarRow", [&](void* row, const auto& fields) {
        AvatarCatalogRow out{Read<std::uint32_t>(row, Field(fields, "AvatarID", "System.UInt32")),
                             this->String(row, fields, "JsonPath"),
                             this->String(row, fields, "DefaultAvatarModelPath")};
        if (!initial_skins.count(out.avatar) &&
            std::any_of(output.players.rows.begin(), output.players.rows.end(),
                        [&](const auto& player) { return player.avatar == out.avatar; }) &&
            std::any_of(fields.begin(), fields.end(), [](const auto& field) {
                return field.name == "AvatarInitialSkinName" && field.type == "RPG.Client.TextID";
            })) {
            try {
                initial_skins.emplace(out.avatar, display.Localize(row, fields, "AvatarInitialSkinName"));
            } catch (const std::exception& error) {
                output.players.issues.push_back({-1, "player display default skin", error.what()});
            }
        }
        return out;
    });
    struct Names {
        std::uint32_t role = 0;
        CharacterGender gender = CharacterGender::Unspecified;
        std::string name, path, skin;
    };
    std::map<std::uint32_t, Names> names;
    for (auto& player : output.players.rows) {
        auto found = names.find(player.avatar);
        if (found == names.end()) {
            Names value;
            auto read = [&](const char* operation, auto body) {
                try {
                    body();
                } catch (const std::exception& error) {
                    output.players.issues.push_back({-1, operation, error.what()});
                }
            };
            read("player display RoleID", [&] { value.role = display.Role(player.avatar); });
            read("player display gender", [&] { value.gender = display.Gender(player.avatar); });
            read("player display name", [&] { value.name = display.Name(player.avatar); });
            read("player display path", [&] { value.path = display.PathName(player.avatar); });
            const auto skin = initial_skins.find(player.avatar);
            if (skin != initial_skins.end())
                value.skin = skin->second;
            found = names.emplace(player.avatar, std::move(value)).first;
        }
        player.role = found->second.role;
        player.gender = found->second.gender;
        player.name = found->second.name;
        player.path_name = found->second.path;
        player.default_skin_name = found->second.skin;
    }
    return output;
}
TextInput CatalogSchema::ReadAdventureIdentity(const std::string& path) {
    TextInput output;
    Scope scope(*this);
    try {
        auto config = this->Config("AdventureCharacterConfig", path, scope);
        output.value = this->String(config, this->metadata.ReadObjectFields(config), "FreeStyleCharacterID");
        output.available = true;
    } catch (const std::exception& error) {
        output.error = error.what();
    }
    return output;
}
PresentationConfigInput CatalogSchema::ReadPresentationConfig(const std::string& path) {
    PresentationConfigInput output;
    output.path = path;
    Scope scope(*this);
    try {
        auto config = this->Config("CharacterConfig", path, scope);
        auto fields = this->metadata.ReadObjectFields(config);
        output = this->Presentation(config, fields, path);
    } catch (const std::exception& error) {
        output.initial_state.error = output.event_files.error = output.disabled_event_layers.error = error.what();
    }
    return output;
}
}
