#include <hksr/animation/OfficialCatalog.hpp>
#include "../../adapters/animation/catalog/CatalogRevisionKey.hpp"
#include <Windows.h>
#include <map>
#include <stdexcept>
#include <memory>
#include <vector>
#include <cstddef>

namespace {
using namespace hksr::api;
void Check(bool value) {
    if (!value)
        throw std::runtime_error("official catalog test failed");
}
enum Kind {
    Text,
    Table,
    Values,
    List,
    Array,
    Npc,
    Player,
    Avatar,
    Config,
    Type,
    MotionList,
    Motion,
    FreeStyleConfig,
    StatesConfig,
    StateRow,
    Number,
    Version,
    VersionField,
    TextId,
    DisplayField,
    DisplayMethod,
    DisplayPathObject,
    DisplaySkinObject
};
struct Node {
    Kind kind = Text;
    std::wstring text;
    std::vector<void*> items;
    std::uint32_t id = 0, avatar = 0, enhanced = 0, skin = 0;
    std::uint64_t hash64 = 0;
    Node *config = nullptr, *prefab = nullptr, *identity = nullptr, *initial = nullptr, *events = nullptr,
         *layers = nullptr;
    bool bad = false;
};
std::vector<std::unique_ptr<Node>> storage;
Node *npcs, *players, *avatars, *config, *object_type, *motion_list, *motion, *shared_motion, *freestyle_config,
    *states_config;
Node *version_data, *version_type, *version_field, *build_id;
Node *display_owner, *display_text_type, *display_array_type, *system_type, *node_type;
Node *display_path_object, *display_skin_object;
std::wstring display_language = L"zh-CN";
bool display_path_available = false, display_name_available = true, display_language_available = true;
int display_invokes = 0;
int info, sequence = 0, loads = 0, copies = 0, frees = 0;
bool npc_unavailable = false, free_fault = false;
bool state_lists = false, motion_index_unavailable = false;
std::map<int, void*> roots;
Node* New(Kind kind) {
    storage.push_back(std::make_unique<Node>());
    storage.back()->kind = kind;
    return storage.back().get();
}
Node* String(const wchar_t* text) {
    auto value = New(Text);
    value->text = text;
    return value;
}
template <class T> Method M(T f) {
    return {reinterpret_cast<void*>(f), &info};
}
int __fastcall Retain(void* object, int handle, int kind, void* metadata) {
    Check(!handle && kind == 2 && metadata == &info);
    roots[++sequence] = object;
    return sequence;
}
void* __fastcall Target(int handle, void*) {
    return roots.at(handle);
}
void __fastcall Free(int handle, void*) {
    ++frees;
    if (free_fault) {
        free_fault = false;
        RaiseException(0xE0550001, 0, 0, nullptr);
    }
    roots.erase(handle);
}
void* __fastcall Npcs(void*) {
    if (npc_unavailable)
        RaiseException(0xE0550002, 0, 0, nullptr);
    return npcs;
}
void* __fastcall Avatars(void*) {
    return avatars;
}
void* __fastcall Players(bool option, void* metadata) {
    Check(!option && metadata == &info);
    return players;
}
void* __fastcall ValueItems(Node* dictionary, void*) {
    return dictionary->config;
}
int __fastcall Count(Node* value, void*) {
    return static_cast<int>(value->items.size());
}
void* __fastcall Item(Node* value, int index, void*) {
    return value->items.at(index);
}
void* __fastcall Create(void* type, int count, void*) {
    Check(type == object_type || type == system_type);
    auto n = New(Array);
    n->items.resize(count);
    return n;
}
void __fastcall Set(Node* array, Node* value, int index, void*) {
    array->items.at(index) = value;
}
void* __fastcall ObjectType(Node* value, void*) {
    Check(value->kind == Array);
    return display_array_type;
}
void* __fastcall DisplayFieldValue(Node* field, Node* receiver, void*) {
    Check(field->kind == DisplayField && receiver->initial);
    return receiver->initial;
}
void* __fastcall DisplayMethodInfo(Node* owner, Node* name, Node* arguments, void*) {
    if (owner == node_type) {
        Check(name->text == L"get_Name" && arguments->items.empty());
        auto method = New(DisplayMethod);
        method->id = 1;
        return method;
    }
    Check(owner == display_owner && name->text == L"GetText" &&
          arguments->items == std::vector<void*>({display_text_type, display_array_type}));
    return New(DisplayMethod);
}
void* __fastcall DisplayInvoke(Node* method, void* receiver, Node* arguments, void*) {
    if (method->id == 1) {
        Check(receiver && arguments->items.empty());
        return static_cast<Node*>(receiver)->initial;
    }
    Check(method->kind == DisplayMethod && !receiver && arguments->items.size() == 2);
    const auto text = static_cast<Node*>(arguments->items[0]);
    const auto parameters = static_cast<Node*>(arguments->items[1]);
    Check(text->kind == TextId && parameters->kind == Array && parameters->items.empty());
    ++display_invokes;
    auto result = New(Text);
    result->text = text->text;
    return result;
}
void __fastcall Copy(Node* values, Node* array, int index, void*) {
    Check(index == 0 && values->items.size() == array->items.size());
    ++copies;
    array->items = values->items;
}
int __fastcall Length(Node* value, void*) {
    return static_cast<int>(value->text.size());
}
std::uint16_t __fastcall Character(Node* value, int index, void*) {
    return static_cast<std::uint16_t>(value->text.at(index));
}
std::uint32_t __fastcall ID(Node* row, void*) {
    return row->id;
}
std::uint32_t __fastcall AvatarID(Node* row, void*) {
    return row->avatar;
}
std::uint32_t __fastcall Enhanced(Node* row, void*) {
    return row->enhanced;
}
std::uint32_t __fastcall Skin(Node* row, void*) {
    return row->skin;
}
std::uint32_t __fastcall DisplayRole(std::uint32_t avatar, void*) {
    Check(avatar == 777);
    return 700;
}
int __fastcall DisplayGender(std::uint32_t avatar, void*) {
    Check(avatar == 777);
    return 2;
}
void* __fastcall DisplayName(std::uint32_t avatar, void*) {
    Check(avatar == 777);
    if (!display_name_available)
        RaiseException(0xE0550004, 0, 0, nullptr);
    return String(L"官方名称");
}
void* __fastcall DisplayPath(std::uint32_t avatar, void*) {
    Check(avatar == 777);
    return display_path_object;
}
void* __fastcall DisplaySkin(std::uint32_t skin, void*) {
    Check(skin == 111);
    return display_skin_object;
}
void* __fastcall TextLanguage(void*) {
    if (!display_language_available)
        RaiseException(0xE0550005, 0, 0, nullptr);
    return String(display_language.c_str());
}
void* __fastcall NpcDisplayRow(std::uint32_t id, void*) {
    for (auto value : npcs->config->items) {
        const auto row = static_cast<Node*>(value);
        if (row->id == id && !row->bad)
            return row;
    }
    return nullptr;
}
void* __fastcall AvatarDisplayRow(std::uint32_t id, void*) {
    for (auto value : avatars->config->items) {
        const auto row = static_cast<Node*>(value);
        if (row->avatar == id)
            return row;
    }
    return nullptr;
}
void* __fastcall Path(Node* row, void*) {
    return row->config;
}
void* __fastcall Prefab(Node* row, void*) {
    return row->prefab;
}
void* __fastcall Identity(Node* row, void*) {
    return row->identity;
}
void* __fastcall Load(Node* path, void*) {
    Check(path && path->kind == Text);
    ++loads;
    return config;
}
void* __fastcall MotionNames(Node* character, void*) {
    Check(character->text == L"CharacterA");
    if (motion_index_unavailable)
        RaiseException(0xE0550003, 0, 0, nullptr);
    return motion_list;
}
std::uint32_t __fastcall MotionID(Node*, Node* name, void*) {
    return name->text == L"IndexedName" ? 42u : 43u;
}
void* __fastcall MotionRow(std::uint32_t id, void*) {
    return id == 42 ? motion : id == 44 ? shared_motion : nullptr;
}
void* __fastcall FreeStyleConfigLoad(Node* path, void*) {
    Check(path->text == L"FreeStyle/A");
    return freestyle_config;
}
void* __fastcall StatesConfigLoad(Node* path, void*) {
    Check(path->text == L"States/A");
    return states_config;
}
std::uint32_t __fastcall NumberValue(Node* node, void*) {
    Check(node->kind == Number);
    return node->id;
}
void* __fastcall BuildId(void*) {
    return build_id;
}
void* __fastcall VersionFieldRead(Node* type, Node* name, int flags, void*) {
    if (type == node_type) {
        Check(flags == 54);
        auto field = New(DisplayField);
        field->text = name->text;
        return field;
    }
    Check(type == version_type && name->text == L"s_VersionData" && flags == 56);
    return version_field;
}
void* __fastcall VersionValue(Node* field, void* instance, void*) {
    Check(field == version_field && !instance);
    return version_data;
}
std::uint32_t __fastcall SelectedDesign(Node* value, void*) {
    Check(value == version_data->config);
    return value->id;
}
struct Inputs : MethodResolver, ObjectMetadata, ManagedObjects {
    Method Resolve(const char* space, const char* type, const char* name, const char* result,
                   std::initializer_list<const char*> args, MethodKind kind) override {
        const std::string t = type, n = name, r = result;
        if (t == "Object" && n == "GetType") {
            Check(kind == MethodKind::Instance && r == "System.Type");
            return M(ObjectType);
        }
        if (t == "Type" && n == "GetMethod") {
            Check(kind == MethodKind::Instance && args.size() == 2);
            return M(DisplayMethodInfo);
        }
        if (t == "MethodBase" && n == "Invoke") {
            Check(std::string(space) == "System.Reflection" && kind == MethodKind::Instance);
            return M(DisplayInvoke);
        }
        if (t == "AvatarHelper" && std::string(space) == "RPG.Client") {
            Check(kind == MethodKind::Static && args.size() == 1 && std::string(*args.begin()) == "System.UInt32");
            if (n == "GetRoleID") {
                Check(r == "System.UInt32");
                return M(DisplayRole);
            }
            if (n == "GetGender") {
                Check(r == "RPG.GameCore.GenderType");
                return M(DisplayGender);
            }
            if (n == "GetAvatarName") {
                Check(r == "System.String");
                return M(DisplayName);
            }
            if (n == "GetAvatarPath" && display_path_available) {
                Check(r == "RPG.AvatarSystem.AvatarPath");
                return M(DisplayPath);
            }
            throw std::runtime_error("optional display path unavailable in fixture");
        }
        if (t == "AvatarSkinHelper") {
            Check(std::string(space) == "RPG.Client" && n == "GetSkinBySkinID" &&
                  r == "RPG.AvatarSystem.AvatarSkin" && kind == MethodKind::Static);
            return M(DisplaySkin);
        }
        if (t == "VersionData") {
            Check(n == "get_BuildId" && r == "System.String" && kind == MethodKind::Static && args.size() == 0);
            return M(BuildId);
        }
        if (t == "RuntimeType") {
            Check(n == "GetField" && kind == MethodKind::Instance && args.size() == 2);
            return M(VersionFieldRead);
        }
        if (t == "PKKGCBHDFNK") {
            Check(n == "GMFJPGKJAAI" && r == "System.UInt32" && kind == MethodKind::Instance && args.size() == 0);
            return M(SelectedDesign);
        }
        if (t == "FreeStyleComponent") {
            Check(kind == MethodKind::Instance && std::string(space) == "RPG.GameCore" && r == "System.String" &&
                  args.size() == 0);
            if (n == "get_FreeStyleCharacterID")
                return M(Identity);
            Check(n == "get_FreeStyleCharacterConfigPath");
            return M(Path);
        }
        Check(kind == MethodKind::Static);
        if (t == "Convert") {
            Check(std::string(space) == "System" && n == "ToUInt32" && r == "System.UInt32" && args.size() == 1 &&
                  std::string(*args.begin()) == "System.Object");
            return M(NumberValue);
        }
        if (t == "AJKNMKCMKHB") {
            Check(std::string(space).empty());
            if (n == "KCPFICBNOMD") {
                Check(r == "System.Collections.Generic.List<System.String>" && args.size() == 1);
                return M(MotionNames);
            }
            if (n == "MCKEFAANJNI") {
                Check(r == "System.UInt32" && args.size() == 2);
                return M(MotionID);
            }
            Check(n == "LMBKNCAIDLC" && r == "RPG.GameCore.FreeStyleMotionConfigRow" && args.size() == 1);
            return M(MotionRow);
        }
        Check(std::string(space) == "RPG.GameCore");
        if (t == "TextmapExcelTable") {
            Check(n == "GetLanguage" && r == "System.String" && args.size() == 0);
            return M(TextLanguage);
        }
        if (t == "NPCDataExcelTable") {
            if (n == "GetData") {
                Check(r == "RPG.GameCore.NPCDataRow" && args.size() == 1 &&
                      std::string(*args.begin()) == "System.UInt32");
                return M(NpcDisplayRow);
            }
            Check(n == "get_dataDict" && args.size() == 0 &&
                  r == "System.Collections.Generic.Dictionary<RPG.GameCore.CommonIndexKey,RPG.GameCore.NPCDataRow>");
            return M(Npcs);
        }
        if (t == "AvatarExcelTable") {
            if (n == "GetData") {
                Check(r == "RPG.GameCore.AvatarRow" && args.size() == 1 &&
                      std::string(*args.begin()) == "System.UInt32");
                return M(AvatarDisplayRow);
            }
            Check(n == "get_dataDict" && args.size() == 0 &&
                  r == "System.Collections.Generic.Dictionary<RPG.GameCore.CommonIndexKey,RPG.GameCore.AvatarRow>");
            return M(Avatars);
        }
        if (t == "AdventurePlayerCommonWrap") {
            Check(n == "GetAllWrapData" && args.size() == 1 && std::string(*args.begin()) == "System.Boolean");
            return M(Players);
        }
        Check(t == "GameCoreConfigLoader" && args.size() == 1 && std::string(*args.begin()) == "System.String");
        if (n == "LoadFreeStyleCharacterConfig") {
            Check(r == "RPG.GameCore.FreeStyleCharacterConfig");
            return M(FreeStyleConfigLoad);
        }
        if (n == "LoadFreeStyleStateConfig") {
            Check(r == "RPG.GameCore.FreeStyleStateConfig");
            return M(StatesConfigLoad);
        }
        Check((n == "LoadAdventureCharacterConfig" && r == "RPG.GameCore.AdventureCharacterConfig") ||
              (n == "LoadCharacterConfig" && r == "RPG.GameCore.CharacterConfig"));
        return M(Load);
    }
    DeclaredType ReadObjectType(void* object) override {
        auto node = static_cast<Node*>(object);
        DeclaredType type{"Test", "Node", {}};
        auto add = [&](const char* n, const char* r, std::vector<std::string> args, Method m) {
            type.methods.push_back({n, r, std::move(args), false, m});
        };
        if (node->kind == VersionField) {
            add("GetValue", "System.Object", {"System.Object"}, M(VersionValue));
            return type;
        }
        if (node->kind == DisplayField) {
            add("GetValue", "System.Object", {"System.Object"}, M(DisplayFieldValue));
            return type;
        }
        if (node->kind == DisplayPathObject || node->kind == DisplaySkinObject)
            return type;
        if (node->kind == Table)
            add("get_Values", "Test.ConstructedValues", {}, M(ValueItems));
        else if (node->kind == Values) {
            add("get_Count", "System.Int32", {}, M(Count));
            add("System.Collections.ICollection.CopyTo", "System.Void", {"System.Array", "System.Int32"}, M(Copy));
        } else if (node->kind == List || node->kind == MotionList) {
            add("get_Count", "System.Int32", {}, M(Count));
            add("get_Item", node->kind == List ? "RPG.GameCore.IAdventurePlayerCommonRowWrap" : "System.String",
                {"System.Int32"}, M(Item));
        } else if (node->kind == Player) {
            add("get_ID", "System.UInt32", {}, M(ID));
            add("get_AvatarID", "System.UInt32", {}, M(AvatarID));
            add("get_EnhancedID", "System.UInt32", {}, M(Enhanced));
            add("get_DevSkinID", "System.UInt32", {}, M(Skin));
            add("get_PlayerJsonPath", "System.String", {}, M(Path));
            add("get_PlayerPrefabPath", "System.String", {}, M(Prefab));
        } else
            throw std::runtime_error("unexpected object declaration request");
        return type;
    }
    std::vector<DeclaredField> ReadObjectFields(void* object) override {
        auto node = static_cast<Node*>(object);
        std::vector<DeclaredField> fields;
        auto add = [&](const char* name, const char* type, std::size_t offset) {
            fields.push_back({"Test", "Node", name, type, offset, 0, !node->bad});
        };
        if (node->kind == Version)
            add("UpdateVersionInfoIns", "PKKGCBHDFNK", offsetof(Node, config));
        if (node->kind == Npc) {
            add("ID", "System.UInt32", offsetof(Node, id));
            add("JsonPath", "System.String", offsetof(Node, config));
            add("ConfigEntityPath", "System.String", offsetof(Node, prefab));
        }
        if (node->kind == Avatar) {
            add("AvatarID", "System.UInt32", offsetof(Node, avatar));
            add("JsonPath", "System.String", offsetof(Node, config));
            add("DefaultAvatarModelPath", "System.String", offsetof(Node, prefab));
        }
        if (node->initial && node->kind == Npc)
            add("DefaultNPCName", "RPG.Client.TextID", offsetof(Node, initial));
        if (node->initial && node->kind == Avatar)
            add("AvatarInitialSkinName", "RPG.Client.TextID", offsetof(Node, initial));
        if (node->kind == TextId) {
            add("hash", "System.Int32", offsetof(Node, id));
            add("hash64", "System.UInt64", offsetof(Node, hash64));
        }
        if (node->kind == DisplayPathObject)
            add("<Name>k__BackingField", "RPG.Client.TextID", offsetof(Node, initial));
        if (node->kind == Motion) {
            add("ID", "System.UInt32", offsetof(Node, id));
            add("StartMotion", "System.String", offsetof(Node, initial));
            add("FreeStyleCharacterID", "System.String", offsetof(Node, identity));
            add("StartMotionPath", "System.String", offsetof(Node, config));
            // Optional loop path deliberately absent: do not drop a valid ID.
        }
        if (node->kind == FreeStyleConfig)
            add("FreeStyleStateConfigPath", "System.String", offsetof(Node, config));
        if (node->kind == StatesConfig)
            add("StateList",
                state_lists ? "System.Collections.Generic.List<RPG.GameCore.State>" : "RPG.GameCore.State[]",
                offsetof(Node, events));
        if (node->kind == StateRow) {
            add("MotionID", "System.UInt32", offsetof(Node, id));
            add("SameAsMotionIDOnLoop", "System.UInt32", offsetof(Node, avatar));
            add("EntranceFreeStyleIDList",
                state_lists ? "System.Collections.Generic.List<System.UInt32>" : "System.UInt32[]",
                offsetof(Node, events));
        }
        if (node->kind == Config) {
            add("FreeStyleCharacterID", "System.String", offsetof(Node, identity));
            if (!node->bad)
                add("InitAnimStateName", "System.String", offsetof(Node, initial));
            fields.push_back(
                {"Test", "Node", "AnimEventConfigList", "System.String[]", offsetof(Node, events), 0, true});
            fields.push_back(
                {"Test", "Node", "DisableAnimEventLayers", "System.String[]", offsetof(Node, layers), 0, true});
        }
        return fields;
    }
    ObjectResult NewStringUtf8(const char* value) override {
        auto n = New(Text);
        while (*value)
            n->text.push_back(*value++);
        return {{true, 0}, n};
    }
    ObjectResult AllocateObject(const char*, const char*) override {
        throw std::runtime_error("catalog must not allocate entity/config objects");
    }
    ObjectResult ManagedType(const char* space, const char* name) override {
        const std::string ns = space, n = name;
        if (ns == "Test" && n == "Node")
            return {{true, 0}, node_type};
        if (ns == "RPG.Client" && n == "TextmapStatic")
            return {{true, 0}, display_owner};
        if (ns == "RPG.Client" && n == "TextID")
            return {{true, 0}, display_text_type};
        if (ns == "System" && n == "Type")
            return {{true, 0}, system_type};
        if (std::string(space) == "RPG.Client" && std::string(name) == "GlobalVars")
            return {{true, 0}, version_type};
        Check(std::string(space) == "System" && std::string(name) == "Object");
        return {{true, 0}, object_type};
    }
};
}
void TestOfficialCatalog() {
    storage.clear();
    roots.clear();
    sequence = loads = copies = frees = display_invokes = 0;
    npc_unavailable = free_fault = false;
    display_path_available = false;
    display_name_available = display_language_available = true;
    display_language = L"zh-CN";
    display_owner = New(Type);
    display_text_type = New(Type);
    display_array_type = New(Type);
    system_type = New(Type);
    node_type = New(Type);
    object_type = New(Type);
    npcs = New(Table);
    avatars = New(Table);
    players = New(List);
    npcs->config = New(Values);
    avatars->config = New(Values);
    auto npc = New(Npc);
    npc->id = 999;
    npc->config = String(L"NPC/藿藿.json");
    npc->prefab = String(L"NPC/Model.json");
    auto bad = New(Npc);
    bad->bad = true;
    npcs->config->items = {bad, npc};
    auto player = New(Player);
    player->id = 12;
    player->avatar = 777;
    player->enhanced = 1;
    player->skin = 2;
    player->config = String(L"Player/Huohuo.json");
    player->prefab = String(L"Player/Prefab.prefab");
    players->items = {player};
    auto avatar = New(Avatar);
    avatar->avatar = 777;
    avatar->config = String(L"Battle/Huohuo.json");
    avatar->prefab = String(L"Battle/Prefab.prefab");
    auto display_text = New(TextId);
    display_text->id = 123;
    display_text->hash64 = 456;
    display_text->text = L"官方显示名";
    npc->initial = avatar->initial = display_text; // same official TextID is localized once across row types
    avatars->config->items = {avatar, avatar};     // provider preserves rows, no arbitrary dedup policy
    config = New(Config);
    config->identity = String(L"少年_藿藿");
    config->initial = String(L"StandBy");
    config->events = New(Array);
    config->events->items = {String(L"Event/A"), String(L"Event/B")};
    config->layers = nullptr;
    motion_list = New(MotionList);
    motion_list->items = {String(L"IndexedName"), String(L"MissingRow")};
    motion = New(Motion);
    motion->id = 42;
    motion->initial = String(L"StartName");
    motion->identity = String(L"SharedCharacter");
    motion->config = String(L"Animation/Shared");
    shared_motion = New(Motion);
    *shared_motion = *motion;
    shared_motion->id = 44;
    freestyle_config = New(FreeStyleConfig);
    freestyle_config->config = String(L"States/A");
    states_config = New(StatesConfig);
    states_config->events = New(Values);
    auto state = New(StateRow);
    state->id = 42;
    state->avatar = 44;
    state->events = New(Values);
    auto entrance = New(Number);
    entrance->id = 44;
    auto missing = New(Number);
    missing->id = 45;
    state->events->items = {entrance, missing};
    states_config->events->items = {state};
    Inputs input;
    ComponentConfigMethods text;
    text.string_length = M(Length);
    text.string_character = M(Character);
    OfficialCatalog catalog(input, input, input, {M(Retain), M(Target), M(Free), {}, {}},
                            ManagedArrays({M(Create), M(Count), M(Item), M(Set), {}}), ComponentConfig(text));
    version_data = New(Version);
    version_data->config = New(Number);
    version_data->config->id = 123;
    version_type = New(Type);
    version_field = New(VersionField);
    build_id = String(L"fixture-build");
    const auto version = catalog.ReadActionDataVersion();
    Check(version.build == "fixture-build" && version.design == 123 && roots.empty());
    const auto revision = catalog.ReadCatalogRevision("installed-fixture");
    Check(revision == hksr::api::detail::CatalogRevisionKey(version.build, version.design, "installed-fixture"));
    Check(catalog.ReadCatalogRevision("changed-install") != revision);
    version_data->config->id = 124;
    Check(catalog.ReadCatalogRevision("installed-fixture") != revision);
    version_data->config->id = 123;
    const auto previous_build = build_id;
    build_id = String(L"new-build");
    Check(catalog.ReadCatalogRevision("installed-fixture") != revision);
    build_id = previous_build;
    const auto revision_directory = std::filesystem::temp_directory_path() /
        (L"hksr-catalog-revision-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
    const auto marker = revision_directory / L"StreamingAssets/DesignData/Windows/M_DesignV.bytes";
    std::filesystem::create_directories(marker.parent_path());
    {
        std::ofstream file(marker, std::ios::binary);
        file << "original";
    }
    const auto installed = hksr::api::detail::InstalledCatalogRevision(revision_directory);
    Check(installed == hksr::api::detail::InstalledCatalogRevision(revision_directory));
    {
        std::ofstream file(marker, std::ios::binary);
        file << "updated";
    }
    Check(installed != hksr::api::detail::InstalledCatalogRevision(revision_directory));
    std::filesystem::remove_all(revision_directory);
    version_data->config->id = 0;
    bool version_failed = false;
    try {
        catalog.ReadActionDataVersion();
    } catch (const std::runtime_error&) {
        version_failed = true;
    }
    Check(version_failed && roots.empty());
    version_data->config->id = 123;
    const auto result = catalog.ReadCharacters();
    Check(result.npcs.enumerated && result.npcs.declared_count == 2 && result.npcs.visited_count == 2 &&
          result.npcs.rows.size() == 1 && result.npcs.issues.size() == 1);
    Check(result.npcs.rows[0].character_config == u8"NPC/藿藿.json");
    Check(result.npcs.rows[0].name == u8"官方显示名" && display_invokes == 1);
    Check(result.players.enumerated && result.players.rows[0].avatar == 777 && result.players.rows[0].skin == 2);
    Check(result.players.rows[0].role == 700 && result.players.rows[0].gender == CharacterGender::Female &&
          result.players.rows[0].name == u8"官方名称" && result.players.rows[0].path_name.empty());
    Check(!result.players.issues.empty()); // optional metadata failure does not drop the source
    Check(result.players.rows[0].default_skin_name == u8"官方显示名");
    Check(result.avatars.rows.size() == 2 && loads == 0 && copies == 2 && roots.empty());
    // The display refresh path targets explicit identities, not source tables.
    // Text caches belong to one read, so a later official language is visible.
    const auto display_copies = copies;
    auto language = catalog.ReadTextLanguage();
    Check(language.available && language.value == "zh-CN" && roots.empty());
    display_language = L"en";
    language = catalog.ReadTextLanguage();
    Check(language.available && language.value == "en" && roots.empty());
    display_language_available = false;
    language = catalog.ReadTextLanguage();
    Check(!language.available && !language.error.empty() && roots.empty());
    display_language_available = true;
    auto npc_name = catalog.ReadCharacterDisplayName(CharacterNameKind::Npc, 999);
    Check(npc_name.name.available && npc_name.name.value == u8"官方显示名" && roots.empty());
    display_text->text = L"Official display name";
    npc_name = catalog.ReadCharacterDisplayName(CharacterNameKind::Npc, 999);
    Check(npc_name.name.available && npc_name.name.value == "Official display name" && roots.empty());
    auto absent_name = catalog.ReadCharacterDisplayName(CharacterNameKind::Npc, 123456);
    Check(!absent_name.name.available && !absent_name.name.error.empty() && roots.empty());
    display_path_object = New(DisplayPathObject);
    display_path_object->initial = New(TextId);
    display_path_object->initial->text = L"Preservation";
    display_path_object->initial->id = 789;
    display_path_object->initial->hash64 = 987;
    display_path_available = true;
    display_name_available = false;
    auto player_name = catalog.ReadCharacterDisplayName(CharacterNameKind::Player, 777);
    Check(!player_name.name.available && !player_name.name.error.empty() &&
          player_name.path_name.available && player_name.path_name.value == "Preservation" &&
          player_name.default_skin_name.available && player_name.default_skin_name.value == "Official display name" &&
          roots.empty());
    display_name_available = true;
    display_path_object->initial->text.clear();
    player_name = catalog.ReadCharacterDisplayName(CharacterNameKind::Player, 777);
    Check(player_name.name.available && player_name.path_name.available && player_name.path_name.value.empty() &&
          roots.empty()); // valid empty official display field, not an unsupported interface
    display_skin_object = New(DisplaySkinObject);
    display_skin_object->initial = display_text;
    auto skin_name = catalog.ReadCharacterDisplayName(CharacterNameKind::Skin, 111);
    Check(skin_name.name.available && skin_name.name.value == "Official display name" && roots.empty());
    display_text->text = L"另一语言皮肤";
    skin_name = catalog.ReadCharacterDisplayName(CharacterNameKind::Skin, 111);
    Check(skin_name.name.available && skin_name.name.value == u8"另一语言皮肤" && roots.empty());
    Check(loads == 0 && copies == display_copies);
    display_path_available = false;
    auto motions = catalog.ReadNpcMotionIndex("CharacterA");
    Check(motions.motions.enumerated && motions.motions.declared_count == 2 && motions.motions.rows.size() == 1 &&
          motions.motions.issues.size() == 1);
    Check(motions.motions.rows[0].id == 42 && motions.motions.rows[0].source_character.value == "SharedCharacter" &&
          !motions.motions.rows[0].loop_path.available && roots.empty() && loads == 0);
    auto combined = catalog.ReadNpcMotions("CharacterA", "FreeStyle/A");
    Check(combined.state_list_read && combined.configured_ids == std::vector<std::uint32_t>({42, 44, 45}) &&
          combined.motions.size() == 2 && combined.issues.size() == 1 && roots.empty());
    Check(combined.motions[1].id == 44 && combined.motions[1].indexed_name.empty() &&
          combined.motions[1].source_character.value == "SharedCharacter");
    state_lists = true;
    auto list_combined = catalog.ReadNpcMotions("CharacterA", "FreeStyle/A");
    Check(list_combined.state_list_read && list_combined.motions.size() == 2 &&
          list_combined.configured_ids == combined.configured_ids && roots.empty());
    motion_index_unavailable = true;
    auto only_states = catalog.ReadNpcMotions("CharacterA", "FreeStyle/A");
    Check(!only_states.index.motions.enumerated && only_states.state_list_read && only_states.motions.size() == 2 &&
          roots.empty());
    motion_index_unavailable = false;
    state_lists = false;
    auto own_component = New(Config);
    own_component->identity = String(L"CharacterA");
    own_component->config = String(L"FreeStyle/A");
    auto from_component = catalog.ReadNpcMotionsFromComponent(own_component);
    Check(from_component.character_config == "FreeStyle/A" && from_component.index.character == "CharacterA" &&
          from_component.state_list_read && from_component.motions.size() == 2 && roots.empty());
    auto identity = catalog.ReadAdventureIdentity("Player/Huohuo.json");
    Check(identity.available && identity.value == u8"少年_藿藿" && loads == 1 && roots.empty());
    auto profile = catalog.ReadPresentationConfig("Battle/Huohuo.json");
    Check(profile.initial_state.available && profile.initial_state.value == "StandBy" &&
          profile.event_files.values.size() == 2 && profile.disabled_event_layers.available &&
          profile.disabled_event_layers.values.empty() && roots.empty());
    config->bad = true;
    profile = catalog.ReadPresentationConfig("Battle/Huohuo.json");
    Check(!profile.initial_state.available && profile.event_files.available &&
          profile.disabled_event_layers.available); // independent inputs
    npc_unavailable = true;
    auto partial = catalog.ReadCharacters();
    Check(!partial.npcs.enumerated && partial.players.rows.size() == 1 && partial.avatars.rows.size() == 2 &&
          roots.empty());
    free_fault = true;
    catalog.ReadAdventureIdentity("Player/Huohuo.json");
    Check(catalog.UnresolvedReferences() == 1 && roots.size() == 1);
    const auto prior = frees;
    catalog.ReadAdventureIdentity("Player/Huohuo.json");
    Check(catalog.UnresolvedReferences() == 1 && roots.size() == 1 && frees == prior + 2); // no retry of unknown free
    roots.clear();
    storage.clear();
    npc_unavailable = false;
}
