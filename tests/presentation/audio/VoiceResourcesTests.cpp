#include "../../../src/presentation/audio/VoiceResources.hpp"
#include <Windows.h>
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
using namespace hksr::api;
void Check(bool value, const char* text) {
    if (!value)
        throw std::runtime_error(text);
}
struct Object {
    std::array<unsigned char, 96> bytes{};
    std::string type, text;
};
std::map<void*, std::unique_ptr<Object>> objects;
std::map<int, void*> roots;
int next_root = 0, row_reads = 0, formats = 0, enum_reads = 0;
void *row = nullptr, *empty_row = nullptr, *audio = nullptr, *config = nullptr, *templates = nullptr;
bool exists_result = true, missing_template = false;
void* Make(std::string type, std::string text = {}) {
    auto value = std::make_unique<Object>();
    value->type = std::move(type);
    value->text = std::move(text);
    auto* pointer = value->bytes.data();
    objects.emplace(pointer, std::move(value));
    return pointer;
}
template <class T> void Put(void* object, std::size_t offset, T value) {
    std::memcpy(static_cast<char*>(object) + offset, &value, sizeof(value));
}
template <class T> Method M(T function) {
    return {reinterpret_cast<void*>(function), nullptr};
}
int __fastcall Keep(void* object, int, int, void*) {
    roots[++next_root] = object;
    return next_root;
}
void* __fastcall Target(int id, void*) {
    return roots.at(id);
}
void __fastcall Free(int id, void*) {
    roots.erase(id);
}
int __fastcall Length(void* object, void*) {
    return static_cast<int>(objects.at(object)->text.size());
}
std::uint16_t __fastcall Character(void* object, int at, void*) {
    return static_cast<std::uint16_t>(objects.at(object)->text.at(static_cast<std::size_t>(at)));
}
void* __fastcall Avatar(std::uint32_t id, void*) {
    ++row_reads;
    Check(id == 1217 || id == 777, "voice lookup did not use supplied Avatar ID");
    return id == 1217 ? row : empty_row;
}
void* __fastcall Parse(void* type, void* name, bool ignore_case, void*) {
    ++enum_reads;
    Check(!ignore_case, "enum names changed case policy");
    const auto& owner = objects.at(type)->text;
    const auto& text = objects.at(name)->text;
    int raw = 0;
    if (owner == "SkillType") {
        Check(text == "Ultra", "wrong native skill semantic");
        raw = 23;
    } else {
        Check(owner == "CharacterVOType", "wrong voice enum owner");
        raw = text == "LightHit" ? 108 : text == "UltraReady" ? 117 : 0;
        Check(raw != 0, "wrong native voice cue");
    }
    auto* value = Make("Enum");
    Put(value, 20, raw);
    return value;
}
void* __fastcall Field(void* owner, void* name, int flags, void*) {
    Check(objects.at(owner)->text == "GlobalVars" && objects.at(name)->text == "s_AudioManager" && flags == 56,
          "unexpected global lookup");
    return Make("FieldInfo");
}
void* __fastcall Value(void*, void* receiver, void*) {
    Check(!receiver, "static AudioManager field receiver");
    return audio;
}
void* __fastcall Template(void* receiver, std::int32_t cue, void*) {
    Check(receiver == templates && (cue == 108 || cue == 117), "voice template enum was hardcoded");
    return missing_template
               ? nullptr
               : Make("String", cue == 108 ? "Ev_vo_avatar_hit_light_{0}" : "Ev_vo_avatar_ultra_select_{0}");
}
void* __fastcall Format(void* pattern, void* tag, void*) {
    ++formats;
    auto result = objects.at(pattern)->text;
    const auto at = result.find("{0}");
    Check(at != std::string::npos && objects.at(tag)->text == "huohuo", "official formatter inputs differ");
    result.replace(at, 3, objects.at(tag)->text);
    return Make("String", std::move(result));
}
bool __fastcall Exists(void* receiver, void* event, void*) {
    Check(receiver == audio && !objects.at(event)->text.empty(), "event existence receiver");
    return exists_result;
}
struct Provider : MethodResolver, ObjectMetadata, ManagedObjects {
    Method Resolve(const char*, const char* type, const char* name, const char*, std::initializer_list<const char*>,
                   MethodKind kind) override {
        const std::string owner = type, method = name;
        if (owner == "AvatarExcelTable") {
            Check(kind == MethodKind::Static && method == "GetData", "Avatar source binding");
            return M(Avatar);
        }
        if (owner == "Enum") {
            Check(kind == MethodKind::Static && method == "Parse", "enum source binding");
            return M(Parse);
        }
        if (owner == "RuntimeType" && method == "GetField")
            return M(Field);
        if (owner == "String" && method == "Format")
            return M(Format);
        if (owner == "AudioManager" && method == "IsEventExistInPck")
            return M(Exists);
        throw std::runtime_error("unexpected binding: " + owner + "." + method);
    }
    DeclaredType ReadObjectType(void* object) override {
        const auto& type = objects.at(object)->type;
        if (type == "FieldInfo")
            return {"Fixture", "FieldInfo", {{"GetValue", "System.Object", {"System.Object"}, false, M(Value)}}};
        if (type == "VoiceMap")
            return {"Fixture",
                    "VoiceMap",
                    {{"get_Item", "System.String", {"RPG.GameCore.CharacterVOType"}, false, M(Template)}}};
        return {"Fixture", type, {}};
    }
    std::vector<DeclaredField> ReadObjectFields(void* object) override {
        const auto& type = objects.at(object)->type;
        if (type == "AvatarRow")
            return {{"RPG.GameCore", type, "AvatarVOTag", "System.String", 24, 6, true}};
        if (type == "AudioManager")
            return {{"RPG.Client", type, "AudioConfig", "RPG.GameCore.AudioConfig", 32, 6, true}};
        if (type == "AudioConfig")
            return {{"RPG.GameCore", type, "CharacterVOEventMap",
                     "System.Collections.Generic.Dictionary<RPG.GameCore.CharacterVOType,System.String>", 40, 6, true}};
        if (type == "Enum")
            return {{"Fixture", type, "value__", "System.Int32", 20, 6, true}};
        throw std::runtime_error("unexpected field source");
    }
    ObjectResult NewStringUtf8(const char* text) override {
        return {{true, 0}, Make("String", text)};
    }
    ObjectResult ManagedType(const char*, const char* type) override {
        return {{true, 0}, Make("RuntimeType", type)};
    }
    ObjectResult AllocateObject(const char*, const char*) override {
        throw std::runtime_error("voice resources must not allocate scene objects");
    }
};
void Run() {
    Provider p;
    row = Make("AvatarRow");
    empty_row = Make("AvatarRow");
    audio = Make("AudioManager");
    config = Make("AudioConfig");
    templates = Make("VoiceMap");
    Put(row, 24, Make("String", "huohuo"));
    Put(empty_row, 24, static_cast<void*>(nullptr));
    Put(audio, 32, config);
    Put(config, 40, templates);
    ComponentConfigMethods text;
    text.string_length = M(Length);
    text.string_character = M(Character);
    const ReferenceMethods refs{M(Keep), M(Target), M(Free), {}, {}};
    auto access = hksr::api::detail::ResolveVoiceResources(p, p, p, refs, ManagedArrays(ArrayMethods{}),
                                                              ComponentConfig(text));
    Check(!access.ready(3) && access.ready(23) == VoiceCue::UltraReady,
          "Ready classification guessed an index/raw number");
    Check(row_reads == 0 && formats == 0, "Ready enum query depends on unrelated Avatar/config resources");
    Check(access.read(1217, VoiceCue::LightHit) == "Ev_vo_avatar_hit_light_huohuo", "shared light-hit template");
    Check(access.read(1217, VoiceCue::UltraReady) == "Ev_vo_avatar_ultra_select_huohuo",
          "shared ultimate-ready template");
    Check(row_reads == 1 && formats == 2 && enum_reads == 3, "resource values not cached or native names bypassed");
    Check(access.read(777, VoiceCue::LightHit).empty() && formats == 2, "blank official tag invented a voice");
    exists_result = false;
    Check(!access.exists("selected_skin_event"), "absent final skin event not distinguished");
    exists_result = true;
    Check(access.exists("selected_skin_event"), "final selected event existence lost");
    missing_template = true;
    Check(access.read(1217, VoiceCue::UltraReady).empty(), "null native template not preserved");
    Check(access.close() && roots.empty(), "temporary voice roots not released");
    bool rejected = false;
    try {
        access.read(1217, VoiceCue::LightHit);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    Check(rejected, "closed resource reader accepted work");
}
}
int main() {
    try {
        Run();
        std::cout << "Voice resource adapter tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
