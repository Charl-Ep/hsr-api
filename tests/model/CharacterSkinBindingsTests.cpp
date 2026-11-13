#include "../../src/model/CharacterSkinNative.hpp"
#include "../../src/model/ActorSkinBindings.hpp"
#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/entity/EntityCreationSession.hpp>
#include <Windows.h>
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
using namespace hksr::api;
namespace {
void Check(bool v, const char* message) {
    if (!v)
        throw std::runtime_error(message);
}
struct Object {
    std::array<unsigned char, 384> data{};
    std::string ns, type, text;
    std::vector<void*> items;
};
std::map<void*, std::unique_ptr<Object>> objects;
void* Make(std::string ns, std::string type, std::string text = {}) {
    auto o = std::make_unique<Object>();
    o->ns = std::move(ns);
    o->type = std::move(type);
    o->text = std::move(text);
    auto* p = o->data.data();
    objects.emplace(p, std::move(o));
    return p;
}
template <class T> void Put(void* p, std::size_t at, T value) {
    std::memcpy(static_cast<char*>(p) + at, &value, sizeof(value));
}
void* Text(std::string value) {
    return Make("System", "String", std::move(value));
}
void* Strings(std::initializer_list<std::string> values) {
    auto* a = Make("System", "String[]");
    for (const auto& value : values)
        objects.at(a)->items.push_back(Text(value));
    return a;
}
struct Entry {
    std::string owner, name;
};
std::vector<std::unique_ptr<Entry>> entries;
template <class F> Method Fn(F f, std::string owner = {}, std::string name = {}) {
    auto p = std::make_unique<Entry>(Entry{std::move(owner), std::move(name)});
    auto* metadata = p.get();
    entries.push_back(std::move(p));
    return {reinterpret_cast<void*>(f), metadata};
}
void *row = nullptr, *avatar_row = nullptr, *preview = nullptr, *skin_data = nullptr, *effective = nullptr,
     *battle = nullptr, *audio = nullptr, *skin_config = nullptr;
std::map<int, void*> roots;
int next_root = 0;
unsigned disposes = 0, exists_calls = 0;
GameEntity* created_actor = nullptr;
GameEntity* __fastcall CreateActor(void*, void*, void*, void*, bool, void*) {
    return created_actor;
}
bool bad_owner = false, bad_skin = false, fail_mapping = false;
std::string tag = "Skin01";
std::set<std::string> available = {"voice_Skin01"};
bool fail_temporary_free = false;
unsigned failed_frees = 0;
std::function<void()> during_release;
int __fastcall Retain(void* p, int, int, void*) {
    roots[++next_root] = p;
    return next_root;
}
void* __fastcall Target(int id, void*) {
    return roots.at(id);
}
void __fastcall Release(int id, void*) {
    Check(roots.at(id) != preview || disposes > 0, "preview root released without Dispose");
    auto callback = std::move(during_release);
    if (callback)
        callback();
    if (fail_temporary_free && roots.at(id) != skin_data) {
        fail_temporary_free = false;
        ++failed_frees;
        RaiseException(0xe0770012, 0, 0, nullptr);
    }
    roots.erase(id);
}
void* __fastcall TableRow(std::uint32_t id, Entry* m) {
    Check(id == (m->owner == "AvatarSkinExcelTable" ? 1100101u : 1001u), "official lookup used wrong identity");
    return m->owner == "AvatarSkinExcelTable" ? row : avatar_row;
}
void* __fastcall Preview(std::uint32_t id, void*) {
    Check(id == 1100101, "preview used wrong skin");
    disposes = 0;
    return preview;
}
void __fastcall Dispose(void* p, void*) {
    Check(p == preview, "disposed unrelated avatar");
    ++disposes;
}
std::uint32_t __fastcall Dressed(void* p, void*) {
    Check(p == preview, "dressed ID read from wrong object");
    return bad_skin ? 1130301u : 1100101u;
}
void* __fastcall Skin(std::uint32_t id, void*) {
    Check(id == 1100101, "skin factory received avatar ID instead of SkinID");
    return skin_data;
}
std::uint32_t __fastcall Avatar(void* p, void*) {
    Check(p == skin_data, "avatar getter receiver differs");
    return bad_owner ? 1303u : 1001u;
}
void* __fastcall GetString(void*, Entry* m) {
    if (m->name == "get_AudioEventTag")
        return Text(tag);
    if (m->name == "get_AdvPlayerPrefabPath")
        return Text("base/adventure");
    if (m->name == "get_DefaultAvatarModelPath")
        return Text("base/battle");
    if (m->name == "get_CharacterSkinOverrideConfigPath")
        return Text("base/override.json");
    throw std::runtime_error("unknown skin getter");
}
void* __fastcall AdventurePath(void* p, void*) {
    Check(p == preview, "configuration path did not use private preview");
    return Text("base/adventure.json");
}
void* __fastcall Effective(void* p, void*) {
    Check(p == preview, "effective configuration did not use private preview");
    return effective;
}
void* __fastcall Load(void* path, Entry* method) {
    if (method->name == "LoadSkinCharacterOverrideConfig") {
        Check(objects.at(path)->text == "base/override.json", "skin override path changed");
        return skin_config;
    }
    Check(objects.at(path)->text == "base/battle.json", "base action configuration identity changed");
    return battle;
}
void* __fastcall Clone(void* source, void*) {
    Check(source == battle, "unexpected shared clone source");
    auto* value = Make("RPG.GameCore", "CharacterConfig");
    objects.at(value)->data = objects.at(source)->data;
    return value;
}
void __fastcall Apply(void* config, void* target, void*) {
    Check(config == skin_config && target != battle, "skin override modified shared source");
    Put(target, 16, Strings({"base/overridden-battle-events"}));
}
void* __fastcall Map(void* p, void* path, void*) {
    Check(p == skin_data, "mapper receiver differs");
    if (fail_mapping)
        RaiseException(0xe0770011, 0, 0, nullptr);
    const auto& text = objects.at(path)->text;
    if (text == "suppressed")
        return nullptr;
    if (text.compare(0, 5, "base/") == 0)
        return Text("skin/" + text.substr(5));
    return path;
}
int __fastcall Length(void* p, void*) {
    return static_cast<int>(objects.at(p)->items.size());
}
void* __fastcall Item(void* p, int index, void*) {
    return objects.at(p)->items.at(index);
}
int __fastcall StringLength(void* p, void*) {
    return static_cast<int>(objects.at(p)->text.size());
}
char16_t __fastcall Character(void* p, int index, void*) {
    return static_cast<unsigned char>(objects.at(p)->text.at(index));
}
void* __fastcall GetField(void*, void* name, int flags, void*) {
    Check(objects.at(name)->text == "s_AudioManager" && flags == 56, "audio global field binding differs");
    return Make("System.Reflection", "RuntimeFieldInfo");
}
void* __fastcall FieldValue(void*, void* target, void*) {
    Check(!target, "audio manager read was not static");
    return audio;
}
bool __fastcall Exists(void* p, void* name, void*) {
    Check(p == audio, "wrong audio manager");
    ++exists_calls;
    return available.count(objects.at(name)->text) != 0;
}
struct Provider : MethodResolver, ObjectMetadata, ManagedObjects {
    Method Resolve(const char*, const char* owner, const char* name, const char*, std::initializer_list<const char*>,
                   MethodKind) override {
        const std::string n = name;
        if (n == "Dispose")
            return Fn(Dispose, owner, n);
        if (n == "GetData")
            return Fn(TableRow, owner, n);
        if (n == "GetSkinPreviewAvatar")
            return Fn(Preview);
        if (n == "GetDressedSkinID")
            return Fn(Dressed);
        if (n == "CreateByAvatarSkinId")
            return Fn(Skin);
        if (n == "get_AvatarId")
            return Fn(Avatar);
        if (n == "MapToSkinRes")
            return Fn(Map);
        if (n == "get_AudioEventTag" || n == "get_AdvPlayerPrefabPath" || n == "get_DefaultAvatarModelPath" ||
            n == "get_CharacterSkinOverrideConfigPath")
            return Fn(GetString, owner, n);
        if (n == "GetAdventureJsonConfigPath")
            return Fn(AdventurePath);
        if (n == "GetModifiedAdventureCharacterConfig")
            return Fn(Effective);
        if (n == "LoadCharacterConfig" || n == "LoadSkinCharacterOverrideConfig")
            return Fn(Load, owner, n);
        if (n == "MemberwiseClone")
            return Fn(Clone);
        if (n == "PKADDEPKJFN")
            return Fn(Apply);
        if (n == "IsEventExistInPck")
            return Fn(Exists);
        if (n == "GetField")
            return Fn(GetField);
        throw std::runtime_error("unexpected official method: " + n);
    }
    DeclaredType ReadObjectType(void* p) override {
        const auto& o = *objects.at(p);
        if (o.type == "RuntimeFieldInfo")
            return {o.ns, o.type, {{"GetValue", "System.Object", {"System.Object"}, false, Fn(FieldValue)}}};
        return {o.ns, o.type, {}};
    }
    std::vector<DeclaredField> ReadObjectFields(void* p) override {
        const auto& o = *objects.at(p);
        if (p == row)
            return {{o.ns, o.type, "ID", "System.UInt32", 16, 6, true},
                    {o.ns, o.type, "AvatarID", "System.UInt32", 20, 6, true}};
        if (p == avatar_row)
            return {{o.ns, o.type, "JsonPath", "System.String", 16, 6, true}};
        if (o.type == "AdventureCharacterConfig" || o.type == "CharacterConfig")
            return {{o.ns, o.type, "AnimEventConfigList", "System.String[]", 16, 6, true}};
        throw std::runtime_error("unexpected field read: " + o.type);
    }
    ObjectResult NewStringUtf8(const char* value) override {
        return {{true, 0}, Text(value)};
    }
    ObjectResult AllocateObject(const char*, const char*) override {
        throw std::runtime_error("skin preparation must not allocate scene objects");
    }
    ObjectResult ManagedType(const char* ns, const char* type) override {
        return {{true, 0}, Make(ns, "RuntimeType", type)};
    }
};
}
namespace catalog_fixture {
std::map<std::uint32_t, void*> skins;
void *dictionary = nullptr, *values = nullptr;
std::uint32_t failed_skin = 0;
std::vector<std::uint32_t> queried;
unsigned translated = 0;
void* Box(std::uint32_t id, std::string text) {
    auto* value = Make("RPG.Client", "TextID", std::move(text));
    Put(value, 16, static_cast<std::int32_t>(id));
    Put(value, 24, static_cast<std::uint64_t>(id));
    return value;
}
void* __fastcall Data(void*) {
    return dictionary;
}
void* __fastcall Values(void* receiver, void*) {
    Check(receiver == dictionary, "skin directory receiver differs");
    return values;
}
void __fastcall Copy(void* receiver, void* array, int start, void*) {
    Check(receiver == values && start == 0, "skin directory copy differs");
    objects.at(array)->items = objects.at(receiver)->items;
}
void* __fastcall ConfigSkin(std::uint32_t id, void*) {
    queried.push_back(id);
    return skins.at(id);
}
void* __fastcall Create(void*, int count, void*) {
    auto* value = Make("System", "Object[]");
    objects.at(value)->items.resize(count);
    return value;
}
void __fastcall Set(void* array, void* value, int index, void*) {
    objects.at(array)->items.at(index) = value;
}
void* __fastcall ArrayType(void*, void*) {
    return Make("System", "RuntimeType", "Object[]");
}
void* __fastcall FindMethod(void* owner, void* name, void* arguments, void*) {
    const auto& type = objects.at(owner)->text;
    const auto& method = objects.at(name)->text;
    Check((type == "TextmapStatic" && method == "GetText" && objects.at(arguments)->items.size() == 2) ||
              (type == "AvatarSkin" && method == "get_Name" && objects.at(arguments)->items.empty()),
          "skin localized property signature differs");
    return Make("System.Reflection", "MonoMethod", method);
}
void* __fastcall Invoke(void* method, void* receiver, void* arguments, void*) {
    const auto& name = objects.at(method)->text;
    if (name == "get_Name") {
        Check(receiver && objects.at(receiver)->type == "AvatarSkin" && objects.at(arguments)->items.empty(),
              "skin name getter receiver differs");
        if (failed_skin && receiver == skins.at(failed_skin))
            RaiseException(0xe0770013, 0, 0, nullptr);
        void* value = nullptr;
        std::memcpy(&value, static_cast<char*>(receiver) + 16, sizeof(value));
        return value;
    }
    Check(name == "GetText" && !receiver && objects.at(arguments)->items.size() == 2, "text-map invocation differs");
    const auto& args = objects.at(arguments)->items;
    Check(objects.at(args[1])->items.empty(), "unexpected localized name format arguments");
    ++translated;
    return Text(objects.at(args[0])->text);
}
void* __fastcall Field(void*, void* name, int flags, void*) {
    Check(objects.at(name)->text == "ActivitySkinName" && flags == 54, "unexpected skin display field");
    return Make("System.Reflection", "RuntimeFieldInfo");
}
void* __fastcall FieldValue(void*, void* receiver, void*) {
    void* value = nullptr;
    std::memcpy(&value, static_cast<char*>(receiver) + 24, sizeof(value));
    return value;
}
struct Provider : MethodResolver, ObjectMetadata, ManagedObjects {
    Method Resolve(const char*, const char* owner, const char* name, const char*, std::initializer_list<const char*>,
                   MethodKind) override {
        const std::string type = owner, n = name;
        if (type == "AvatarSkinExcelTable" && n == "get_dataDict")
            return Fn(Data);
        if (type == "AvatarSkinHelper" && n == "GetSkinBySkinID")
            return Fn(ConfigSkin);
        if (type == "Object" && n == "GetType")
            return Fn(ArrayType);
        if (type == "Type" && n == "GetMethod")
            return Fn(FindMethod);
        if (type == "MethodBase" && n == "Invoke")
            return Fn(Invoke);
        if (type == "RuntimeType" && n == "GetField")
            return Fn(Field);
        throw std::runtime_error("unexpected skin catalog method: " + type + "." + n);
    }
    DeclaredType ReadObjectType(void* value) override {
        const auto& o = *objects.at(value);
        DeclaredType type{o.ns, o.type, {}};
        if (o.type == "Dictionary")
            type.methods.push_back({"get_Values", "Test.Values", {}, false, Fn(Values)});
        if (o.type == "Values") {
            type.methods.push_back({"get_Count", "System.Int32", {}, false, Fn(Length)});
            type.methods.push_back({"System.Collections.ICollection.CopyTo",
                                    "System.Void",
                                    {"System.Array", "System.Int32"},
                                    false,
                                    Fn(Copy)});
        }
        if (o.type == "RuntimeFieldInfo")
            type.methods.push_back({"GetValue", "System.Object", {"System.Object"}, false, Fn(FieldValue)});
        return type;
    }
    std::vector<DeclaredField> ReadObjectFields(void* value) override {
        const auto& o = *objects.at(value);
        if (o.type == "AvatarSkinRow")
            return {{o.ns, o.type, "ID", "System.UInt32", 16, 6, true},
                    {o.ns, o.type, "AvatarID", "System.UInt32", 20, 6, true},
                    {o.ns, o.type, "ActivitySkinName", "RPG.Client.TextID", 24, 6, true}};
        if (o.type == "TextID")
            return {{o.ns, o.type, "hash", "System.Int32", 16, 6, true},
                    {o.ns, o.type, "hash64", "System.UInt64", 24, 6, true}};
        throw std::runtime_error("unexpected skin catalog fields: " + o.type);
    }
    ObjectResult NewStringUtf8(const char* value) override {
        return {{true, 0}, Text(value)};
    }
    ObjectResult ManagedType(const char* ns, const char* type) override {
        return {{true, 0}, Make(ns, "RuntimeType", type)};
    }
    ObjectResult AllocateObject(const char*, const char*) override {
        throw std::runtime_error("skin catalog must not allocate actors");
    }
};
void Test() {
    const std::array<std::uint32_t, 9> ids = {1100101, 1130301, 1131001, 1140701, 1140901,
                                              1141501, 1150101, 1150501, 1151101};
    dictionary = Make("Test", "Dictionary");
    values = Make("Test", "Values");
    skins.clear();
    queried.clear();
    failed_skin = 0;
    translated = 0;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        auto* skin_row = Make("RPG.GameCore", "AvatarSkinRow");
        Put(skin_row, 16, ids[i]);
        Put(skin_row, 20, static_cast<std::uint32_t>(1000 + i));
        Put(skin_row, 24, Box(ids[i] + 100000, i % 2 ? "Unrelated activity" : ""));
        objects.at(values)->items.push_back(skin_row);
        auto* skin = Make("RPG.AvatarSystem", "AvatarSkin");
        Put(skin, 16, Box(ids[i], "Official skin " + std::to_string(i)));
        skins.emplace(ids[i], skin);
    }
    Provider provider;
    ReferenceMethods refs{Fn(Retain), Fn(Target), Fn(Release)};
    ManagedArrays arrays({Fn(Create), Fn(Length), Fn(Item), Fn(Set), {}});
    ComponentConfigMethods text_methods;
    text_methods.string_length = Fn(StringLength);
    text_methods.string_character = Fn(Character);
    auto read = [&] {
        std::function<bool()> cleanup;
        auto result = detail::CharacterSkinFactory::Read(provider, provider, provider, refs, arrays,
                                                         ComponentConfig(text_methods), cleanup);
        Check(cleanup && cleanup() && roots.empty(), "skin catalog retained references escaped");
        return result;
    };
    auto catalog = read();
    Check(catalog.complete && catalog.errors.empty() && catalog.rows.size() == 9,
          "complete skin directory was not preserved");
    Check(queried == std::vector<std::uint32_t>(ids.begin(), ids.end()) && translated == 9,
          "official skin name getter did not cover every directory row");
    for (std::size_t i = 0; i < ids.size(); ++i)
        Check(catalog.rows[i].skin_id == ids[i] && catalog.rows[i].avatar_id == 1000 + i &&
                  catalog.rows[i].name == "Official skin " + std::to_string(i),
              "activity name substituted for official skin name or identity changed");
    failed_skin = ids[4];
    catalog = read();
    Check(catalog.complete && catalog.rows.size() == 9 && catalog.errors.size() == 1,
          "one display failure invalidated or truncated the skin directory");
    for (std::size_t i = 0; i < ids.size(); ++i)
        Check(catalog.rows[i].skin_id == ids[i] && catalog.rows[i].avatar_id == 1000 + i &&
                  catalog.rows[i].name == (i == 4 ? "" : "Official skin " + std::to_string(i)),
              "one failed display name contaminated another skin");
}
}
int main() try {
    row = Make("RPG.GameCore", "AvatarSkinRow");
    Put(row, 16, 1100101u);
    Put(row, 20, 1001u);
    avatar_row = Make("RPG.GameCore", "AvatarRow");
    Put(avatar_row, 16, Text("base/battle.json"));
    preview = Make("RPG.Client", "PreviewAvatarProxy");
    skin_data = Make("RPG.Client", "AvatarExtendSkinData");
    audio = Make("RPG.Client", "AudioManager");
    effective = Make("RPG.GameCore", "AdventureCharacterConfig");
    Put(effective, 16, Strings({"base/adventure-events", "suppressed"}));
    battle = Make("RPG.GameCore", "CharacterConfig");
    Put(battle, 16, Strings({"base/battle-events"}));
    skin_config = Make("RPG.GameCore", "SkinCharacterOverrideConfig");
    auto original = objects.at(battle)->data;
    Provider provider;
    ReferenceMethods refs{Fn(Retain), Fn(Target), Fn(Release)};
    ManagedArrays arrays({{}, Fn(Length), Fn(Item), {}, {}});
    ComponentConfigMethods text_methods;
    text_methods.string_length = Fn(StringLength);
    text_methods.string_character = Fn(Character);
    ComponentConfig text(text_methods);
    auto prepare = [&] {
        return detail::CharacterSkinFactory::Prepare(provider, provider, provider, refs, arrays, text, 1100101);
    };
    auto selected = prepare();
    const auto& s = selected->Status();
    Check(s.prepared && s.errors.empty() && s.skin_id == 1100101 && s.avatar_id == 1001,
          "official skin preparation failed");
    Check(s.adventure_config == "base/adventure.json" && s.battle_config == "base/battle.json",
          "action source identity was replaced");
    Check(s.adventure_model == "skin/adventure" && s.battle_model == "skin/battle",
          "official model mapping not applied");
    Check(s.adventure_event_files == std::vector<std::string>{"skin/adventure-events"} &&
              s.battle_event_files == std::vector<std::string>{"skin/overridden-battle-events"},
          "effective event sources were lost or suppression ignored");
    Check(objects.at(battle)->data == original, "shared battle configuration changed during skin override");
    Check(disposes == 1 && roots.size() == 1, "preview was not disposed once or temporary roots escaped");
    Check(selected->SelectSound("voice") == "voice_Skin01" && selected->SelectSound("unknown") == "unknown" &&
              exists_calls == 2,
          "explicit animation sound existence fallback differs");
    available.insert("voice_Skin01_Skin01");
    Check(selected->SelectSound("voice_Skin01") == "voice_Skin01" && exists_calls == 2,
          "previously selected animation sound received a second tag");
    Check(selected->SelectSound(selected->SelectSound("voice")) == "voice_Skin01" && exists_calls == 3,
          "repeated animation sound selection was not idempotent");
    Check(!selected->MapResource("suppressed"), "official resource suppression lost");
    bool nested_close = true;
    during_release = [&] { nested_close = selected->Close(); };
    Check(selected->MapResource("base/fx") == "skin/fx" && !nested_close && !selected->Status().closed &&
              roots.size() == 1,
          "reentrant Close released in-flight mapping inputs");
    fail_mapping = true;
    bool failed = false;
    try {
        selected->MapResource("base/fx");
    } catch (const std::runtime_error&) {
        failed = true;
    }
    fail_mapping = false;
    Check(failed, "native mapping failure became a successful null suppression");
    // Skin binding owns only its exact actor generation and drains after its
    // disposal, not merely after the caller drops its presentation wrapper.
    EntityOwner actor_owner;
    detail::ActorSkinBindings held(provider, refs, actor_owner, arrays, text);
    auto* actor = reinterpret_cast<GameEntity*>(Make("RPG.GameCore", "GameEntity"));
    auto* world = reinterpret_cast<GameWorld*>(Make("RPG.GameCore", "GameWorld"));
    auto* other_world = reinterpret_cast<GameWorld*>(Make("RPG.GameCore", "OtherWorld"));
    created_actor = actor;
    EntityCreationSession creation(actor_owner, Fn(CreateActor), {}, refs);
    creation.Create(NpcCreateInput{world, reinterpret_cast<EntityManager*>(Make("RPG.GameCore", "EntityManager")),
                                  reinterpret_cast<MapNpcDefinition*>(Make("RPG.GameCore", "MapNpcDef")),
                                  reinterpret_cast<NpcDataRow*>(Make("RPG.GameCore", "NPCDataRow")), true});
    Check(creation.Status().create_returned && creation.Status().entities.size() == 1,
          "skin ownership fixture did not complete its official creation receipt");
    const auto token = creation.Status().entities.front().token;
    unsigned births = 0;
    auto bind = held.BirthObserver(selected, [&](auto origin) {
        Check(origin.token.generation == token.generation, "skin birth provenance changed");
        ++births;
    });
    bind({token, world});
    bind({token, world}); // the synchronous construction receipt is idempotent
    Check(births == 2 && held.Find(actor, world) == selected && !held.Find(actor, other_world),
          "skin binding lost exact world/generation");
    Check(!held.Close() && !selected->Status().closed, "skin closed while actor exit events could still use it");
    TrackedEntities().Disposed(token);
    Check(!held.Find(actor, world), "skin binding accepted an ended actor generation");
    held.Collect();
    Check(held.ActorsDrained() && held.Close() && selected->Status().closed,
          "skin owner did not drain and close retained preparation");
    creation.Service({});
    Check(creation.Status().closed, "skin ownership fixture creation roots did not drain");
    Check(selected->Close() && roots.empty(), "skin GC roots did not close");
    bad_skin = true;
    auto wrong = prepare();
    Check(!wrong->Status().prepared && disposes == 1, "wrong preview selection was accepted or not disposed");
    Check(wrong->Close() && roots.empty(), "failed preview leaked roots");
    bad_skin = false;
    bad_owner = true;
    wrong = prepare();
    Check(!wrong->Status().prepared && disposes == 1, "wrong skin/avatar ownership accepted");
    Check(wrong->Close() && roots.empty(), "wrong owner leaked roots");
    bad_owner = false;
    tag.clear();
    auto plain = prepare();
    const auto before = exists_calls;
    Check(plain->SelectSound("voice") == "voice" && exists_calls == before,
          "empty audio tag queried global audio unnecessarily");
    Check(plain->Close() && roots.empty(), "empty-tag skin did not close");
    fail_temporary_free = true;
    auto pending = prepare();
    Check(pending->Status().prepared && failed_frees == 1 && roots.size() == 2,
          "temporary release failure was not retained separately from skin data");
    Check(!pending->Close() && roots.size() == 1 && failed_frees == 1,
          "pending cleanup disappeared or repeated an uncertain native Free");
    Check(!pending->Close() && roots.size() == 1 && failed_frees == 1,
          "later Close retried an unknown decrement or falsely completed");
    pending.reset();
    roots.clear(); // Test-process teardown, not another native decrement.
    catalog_fixture::Test();
    std::cout << "Official skin lookup, effective events, mapper suppression/failure, audio existence, preview "
                 "disposal and GC ownership passed.\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
