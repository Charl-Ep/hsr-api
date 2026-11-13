#include <hksr/model/ActorVisualConfiguration.hpp>
#include "../../src/model/VisualConfigurationHost.hpp"
#include <hksr/entity/LifecycleHooks.hpp>
#include <Windows.h>
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
using namespace hksr::api;
namespace {
void Check(bool value, const char* text) {
    if (!value)
        throw std::runtime_error(text);
}
template <class F> std::string Failure(F operation) {
    try {
        operation();
    } catch (const std::exception& error) {
        return error.what();
    }
    throw std::runtime_error("expected visual configuration failure");
}
template <class T> T Read(void* p, std::size_t offset) {
    T value{};
    std::memcpy(&value, static_cast<char*>(p) + offset, sizeof(value));
    return value;
}
template <class T> void Write(void* p, std::size_t offset, T value) {
    std::memcpy(static_cast<char*>(p) + offset, &value, sizeof(value));
}
int info = 0, actor = 0, world = 0;
std::map<int, void*> roots;
int next_root = 0;
unsigned free_calls = 0;
bool fail_free_once = false;
int __fastcall Keep(void* p, int, int, void*) {
    roots[++next_root] = p;
    return next_root;
}
void* __fastcall Target(int id, void*) {
    return roots.at(id);
}
void __fastcall Free(int id, void*) {
    ++free_calls;
    if (fail_free_once) {
        fail_free_once = false;
        RaiseException(0xe0770002, 0, 0, nullptr);
    }
    roots.erase(id);
}
template <class F> Method M(F f) {
    return {reinterpret_cast<void*>(f), &info};
}
struct Record {
    std::array<unsigned char, 256> bytes{};
    std::string ns, type, text;
    std::vector<void*> items;
};
struct Field {
    std::string name;
    std::size_t offset;
};
std::map<void*, Field> reflection;
std::map<void*, std::unique_ptr<Record>> records;
void* Make(std::string ns, std::string type, std::string text = {}) {
    auto record = std::make_unique<Record>();
    record->ns = std::move(ns);
    record->type = std::move(type);
    record->text = std::move(text);
    auto* p = record->bytes.data();
    records.emplace(p, std::move(record));
    return p;
}
unsigned discoveries = 0, writes = 0, refreshes = 0;
void* old_effects = nullptr;
void* new_effects = nullptr;
void* default_effects = nullptr;
void* default_events = nullptr;
void* bound_events = nullptr;
std::vector<std::string> refresh_order;
unsigned transitions = 0, blendshapes = 0;
float last_duration = 0;
bool last_enable = false;
bool fail_blend = false;
std::string missing_fields_type, missing_method, missing_type, missing_reflection;
void __fastcall Emotion(void* receiver, int source, bool enable, float time, void*) {
    Check(Read<void*>(receiver, 16) == &actor && source == 0, "emotion actual source/receiver ABI differs");
    ++transitions;
    last_enable = enable;
    last_duration = time;
}
void __fastcall Blend(void*, bool enable, void*) {
    ++blendshapes;
    if (fail_blend)
        RaiseException(0xe0770001, 0, 0, nullptr);
    Check(enable == last_enable, "blendshape enable differs");
}
void* __fastcall Adaptations(void* data, void*) {
    auto* current = Read<void*>(data, 96);
    return current ? current : default_effects;
}
void __fastcall Refresh(void* data, void* old_value, void* new_value, void*) {
    Check(Read<void*>(data, 16) == &actor, "refresh wrong actor");
    ++refreshes;
    old_effects = old_value;
    new_effects = new_value;
    refresh_order.push_back("effects");
}
void* __fastcall Events(void* data, void*) {
    auto* current = Read<void*>(data, 72);
    return current ? current : default_events;
}
void __fastcall BindEvents(void* consumer, void* paths, void* layers, void*) {
    Check(records.at(consumer)->type == "HHECHLOIEJO", "event consumer native ABI differs");
    if (!Read<void*>(consumer, 112))
        return;
    Write(consumer, 176, paths);
    if (layers)
        Write(consumer, 128, layers);
    bound_events = paths;
    refresh_order.push_back("events");
}
std::vector<DeclaredField> Fields(const char* ns, const char* name) {
    ++discoveries;
    const std::string type = name;
    if (type == missing_fields_type)
        return {};
    if (type == "CharacterDataComponent")
        return {{ns, name, "_OverrideCharacterConfigParam", "IJDDNGPOEHK", 32, 4, true},
                {"RPG.GameCore", "GameComponentBase", "_OwnerRef", "RPG.GameCore.GameEntity", 16, 4, true}};
    if (type == "PEPNFLNCDHG")
        return {{"RPG.GameCore", "GameComponentBase", "_OwnerRef", "RPG.GameCore.GameEntity", 16, 4, true}};
    if (type == "IJDDNGPOEHK")
        return {{ns, name, "radius", "System.Single", 40, 6, true},
                {ns, name, "somato", "RPG.GameCore.CharacterSomatoType", 72, 6, true},
                {ns, name, "effects", "RPG.GameCore.EffectAdaptionConfig[]", 80, 6, true},
                {ns, name, "graph", "System.String", 88, 6, true},
                {ns, name, "events", "System.String[]", 56, 6, true}};
    if (type == "Single" || type == "Int32")
        return {{ns, name, "m_value", std::string(ns) + "." + name, 16, 1, true}};
    if (type == "CharacterSomatoType")
        return {{ns, name, "value__", "System.Int32", 16, 6, true}};
    if (type == "HHECHLOIEJO")
        return {{ns, name, "KGGGDHNMDMK", "System.String[]", 176, 1, true},
                {ns, name, "NBNJJDIMDKE", "System.String[]", 128, 1, true},
                {ns, name, "CPIIOIPAAKK", "IBEJMEEEJMB", 112, 1, true}};
    Check(type == "EffectAdaptionConfig", "unexpected fields");
    return {{ns, name, "EffectPath", "System.String", 16, 6, true},
            {ns, name, "AliasName", "System.String", 24, 6, true},
            {ns, name, "AttachPoint", "System.String", 32, 6, true},
            {ns, name, "PositionOffset", "RPG.MVector3", 40, 6, true},
            {ns, name, "RotationOffset", "RPG.MVector3", 52, 6, true},
            {ns, name, "Scale", "System.Single", 64, 6, true}};
}
void* __fastcall FieldGet(void* type, void* name, int flags, void*) {
    Check(flags == 54, "field resolution did not use exact declaring owner");
    const auto& record = *records.at(type);
    const auto& field_name = records.at(name)->text;
    if (field_name == missing_reflection)
        return nullptr;
    auto* field = Make("System.Reflection", "RuntimeFieldInfo");
    for (const auto& f : Fields(record.ns.c_str(), record.text.c_str()))
        if (f.name == field_name) {
            reflection[field] = {f.name, f.offset};
            return field;
        }
    throw std::runtime_error("field missing");
}
void* __fastcall GetValue(void* field, void* target, void*) {
    const auto& f = reflection.at(field);
    if (f.name == "effects" || f.name == "events")
        return Read<void*>(target, f.offset);
    Check(f.name == "_OverrideCharacterConfigParam", "unexpected boxed read");
    auto* copy = Make("", "IJDDNGPOEHK");
    std::memcpy(static_cast<char*>(copy) + 16, static_cast<char*>(target) + f.offset, 136);
    return copy;
}
void __fastcall SetValue(void* field, void* target, void* value, int flags, void* binder, void* culture, void*) {
    Check(!flags && !binder && !culture, "field setter ABI differs");
    ++writes;
    const auto& f = reflection.at(field);
    if (f.name == "_OverrideCharacterConfigParam")
        std::memcpy(static_cast<char*>(target) + f.offset, static_cast<char*>(value) + 16, 136);
    else if (f.name == "radius")
        Write(target, f.offset, Read<float>(value, 16));
    else if (f.name == "somato") {
        Check(records.at(value)->type == "CharacterSomatoType",
              "somato setter received underlying scalar rather than actual enum box");
        Write(target, f.offset, Read<int>(value, 16));
    } else
        Write(target, f.offset, value);
}
void* __fastcall ArrayCreate(void* type, int count, void*) {
    const auto& source = *records.at(type);
    auto* p = Make(source.ns, source.text + "[]");
    records.at(p)->items.resize(count);
    return p;
}
int __fastcall ArrayLength(void* array, void*) {
    return static_cast<int>(records.at(array)->items.size());
}
void* __fastcall ArrayGet(void* array, int index, void*) {
    return records.at(array)->items.at(index);
}
void __fastcall ArraySet(void* array, void* value, int index, void*) {
    records.at(array)->items.at(index) = value;
}
struct Provider : MethodResolver, ObjectMetadata, ManagedObjects {
    Method Resolve(const char*, const char*, const char* name, const char*, std::initializer_list<const char*>,
                   MethodKind) override {
        ++discoveries;
        const std::string n = name;
        if (n == missing_method)
            throw std::runtime_error("synthetic visual method absent: " + n);
        if (n == "GetField")
            return M(FieldGet);
        if (n == "_OnEffectAdaptionListChanged")
            return M(Refresh);
        if (n == "get_EffectAdaptionList")
            return M(Adaptations);
        if (n == "get_AnimEventConfigList")
            return M(Events);
        if (n == "FOGNIIEDKCH")
            return M(BindEvents);
        if (n == "EMFIGLCNJIP")
            return M(Emotion);
        if (n == "HNFHGGINBHF")
            return M(Blend);
        throw std::runtime_error("unexpected method: " + n);
    }
    DeclaredType ReadObjectType(void* p) override {
        const auto& r = *records.at(p);
        if (r.type == "RuntimeFieldInfo")
            return {r.ns,
                    r.type,
                    {{"GetValue", "System.Object", {"System.Object"}, false, M(GetValue)},
                     {"SetValue",
                      "System.Void",
                      {"System.Object", "System.Object", "System.Reflection.BindingFlags", "System.Reflection.Binder",
                       "System.Globalization.CultureInfo"},
                      false,
                      M(SetValue)}}};
        return {r.ns, r.type, {}};
    }
    std::vector<DeclaredField> ReadObjectFields(void* p) override {
        const auto& r = *records.at(p);
        return Fields(r.ns.c_str(), r.type.c_str());
    }
    ObjectResult NewStringUtf8(const char* s) override {
        return {{true, 0}, Make("System", "String", s)};
    }
    ObjectResult AllocateObject(const char* ns, const char* name) override {
        return {{true, 0}, Make(ns, name)};
    }
    ObjectResult ManagedType(const char* ns, const char* name) override {
        if (name == missing_type)
            return {{true, 0}, nullptr};
        return {{true, 0}, Make(ns, "RuntimeType", name)};
    }
};
void *captured_model = nullptr, *captured_data = nullptr, *captured_emotion = nullptr, *captured_event = nullptr;
bool lookup_fault = false;
std::string component_fault_type;
GameWorld* captured_world = reinterpret_cast<GameWorld*>(&world);
std::uint32_t __fastcall ActorId(GameEntity*, void*) {
    return 42;
}
GameWorld* __fastcall ActorWorld(GameEntity*, void*) {
    return captured_world;
}
bool __fastcall Loaded(GameEntity*, void*) {
    return true;
}
void* __fastcall Component(GameEntity*, void* type, void*) {
    if (lookup_fault)
        RaiseException(0xe0770003, 0, 0, nullptr);
    const auto& name = records.at(type)->text;
    if (name == component_fault_type)
        RaiseException(0xe0770004, 0, 0, nullptr);
    if (name == "CharacterModelComponent")
        return captured_model;
    if (name == "CharacterDataComponent")
        return captured_data;
    if (name == "PEPNFLNCDHG")
        return captured_emotion;
    Check(name == "LGBHONLHNPN", "unexpected capture component");
    return captured_event;
}
void TestIndependentBindings(Provider& p, void* data, void* emotion, void* consumer) {
    enum class Feature { Data, Radius, Adaptations, Graph, DataEvents, Events, Emotion, Strings };
    struct Missing {
        Feature feature;
        const char* fields;
        const char* method;
        const char* type;
        const char* reflection;
    };
    const Missing cases[] = {{Feature::Data, "CharacterDataComponent", "", "", ""},
                             {Feature::Radius, "", "", "", "radius"},
                             {Feature::Adaptations, "EffectAdaptionConfig", "", "", ""},
                             {Feature::Graph, "", "", "", "graph"},
                             {Feature::DataEvents, "", "get_AnimEventConfigList", "", ""},
                             {Feature::Events, "HHECHLOIEJO", "", "", ""},
                             {Feature::Emotion, "PEPNFLNCDHG", "", "", ""},
                             {Feature::Strings, "", "", "String", ""}};
    for (const auto& test : cases) {
        missing_fields_type = test.fields;
        missing_method = test.method;
        missing_type = test.type;
        missing_reflection = test.reflection;
        ActorVisualConfigurationFactory factory(
            p, p, p, {M(Keep), M(Target), M(Free), {}, {}},
            ManagedArrays({M(ArrayCreate), M(ArrayLength), M(ArrayGet), M(ArraySet), {}}), Fields);
        missing_fields_type.clear();
        missing_method.clear();
        missing_type.clear();
        missing_reflection.clear();
        auto bind = [&](void* source) {
            return factory.Bind({{{reinterpret_cast<GameEntity*>(&actor), 1}, reinterpret_cast<GameWorld*>(&world)},
                                 11, source, emotion, [] { return true; }, consumer, true});
        };
        auto full = bind(data), story = bind(nullptr);
        Check(full->Current() && story->Current(), "partial adaptation ended common model binding");
        for (const auto use : {VisualConfigurationComponent::Data, VisualConfigurationComponent::Events,
                               VisualConfigurationComponent::Emotion})
            Check(full->Current(use), "adaptation failure was mistaken for component identity replacement");
        const ActorVisualConfigurationInput radius{false, 1.2f, {}, {}, {}, {}};
        if (test.feature == Feature::Data || test.feature == Feature::Radius)
            Check(!Failure([&] { full->Apply(radius); }).empty(), "unadapted radius was applied");
        else
            Check(full->Apply(radius).unsupported_fields.empty(), "unrelated adaptation blocked radius");
        Check(story->Apply(radius).unsupported_fields == std::vector<std::string>{"radius"},
              "actual absent Data was confused with an adaptation error");
        if (test.feature == Feature::Emotion)
            Check(Failure([&] { full->EnableEmotion(true, 0, .2f); }).find("emotion") != std::string::npos,
                  "emotion adaptation failure was hidden");
        else
            Check(full->EnableEmotion(true, 0, .2f), "unrelated adaptation blocked emotion");
        const ActorVisualConfigurationInput events{false, {}, {}, {}, {}, std::vector<std::string>{"partial.json"}};
        if (test.feature == Feature::Events || test.feature == Feature::Strings)
            Check(!Failure([&] { story->Apply(events); }).empty(), "unadapted direct events were applied");
        else {
            Check(story->Apply(events).unsupported_fields.empty(), "unrelated adaptation blocked direct events");
            story->Apply({true, {}, {}, {}, {}, {}});
        }
        if (test.feature == Feature::DataEvents || test.feature == Feature::Events || test.feature == Feature::Strings) {
            const auto before = writes;
            Check(!Failure([&] { full->Apply(events); }).empty() && writes == before,
                  "known event adaptation failure committed a partial Data overlay");
        }
        if (test.feature == Feature::DataEvents)
            Check(full->Apply({true, {}, {}, {}, {}, std::vector<std::string>{"explicit.json"}})
                      .unsupported_fields.empty(),
                  "explicit event reset unnecessarily required the default-path getter");
        if (test.feature == Feature::DataEvents || test.feature == Feature::Events) {
            auto data_only = factory.Bind({{{reinterpret_cast<GameEntity*>(&actor), 1},
                                            reinterpret_cast<GameWorld*>(&world)},
                                           11, data, emotion, [] { return true; }, nullptr, false});
            Check(data_only->Apply(events).unsupported_fields.empty(),
                  "Data-only event override required an absent consumer operation");
            Check(data_only->Close(), "Data-only partial binding did not close");
        }
        if (test.feature == Feature::Adaptations) {
            ActorVisualConfigurationInput effects;
            effects.effect_adaptations = std::vector<EffectAdaptionEntry>{};
            Check(Failure([&] { full->Apply(effects); }).find("effect adaptations") != std::string::npos,
                  "adaptation preparation failure was hidden");
            Check(full->Apply({true, {}, {}, {}, {}, {}}).unsupported_fields.empty(),
                  "clearing effect overrides unnecessarily required constructing adaptation entries");
        }
        if (test.feature == Feature::Graph) {
            ActorVisualConfigurationInput graph;
            graph.graph_emotion = "other.asset";
            Check(Failure([&] { full->Apply(graph); }).find("graph emotion") != std::string::npos,
                  "graph reflection failure was hidden");
        }
        if (test.feature == Feature::Radius)
            Check(full->Apply({false, {}, 7, {}, {}, {}}).unsupported_fields.empty(),
                  "radius reflection failure blocked independent somato");
        if (test.feature == Feature::Strings)
            Check(story->Apply({true, {}, {}, {}, {}, {}}).unsupported_fields.size() == 4,
                  "restoring direct baseline unnecessarily required a new string array");
        Check(full->Close() && story->Close() && factory.Close() && roots.empty(),
              "partial adaptation preparation roots did not drain");
    }
}
void TestIndependentCapture(Provider& p, void* data, void* emotion, void* consumer) {
    const ReferenceMethods refs{M(Keep), M(Target), M(Free), {}, {}};
    const ManagedArrays arrays({M(ArrayCreate), M(ArrayLength), M(ArrayGet), M(ArraySet), {}});
    AnimationEventMethods event_methods;
    event_methods.consumer_offset = 16;
    event_methods.has_consumer = true;
    captured_model = Make("RPG.GameCore", "CharacterModelComponent");
    captured_event = Make("", "LGBHONLHNPN");
    Write(captured_event, 16, consumer);
    captured_data = data;
    captured_emotion = emotion;
    const auto token = TrackedEntities().Track(reinterpret_cast<GameEntity*>(&actor));
    for (bool type_failure : {false, true})
        for (const auto* name : {"CharacterDataComponent", "PEPNFLNCDHG", "LGBHONLHNPN"}) {
            if (type_failure)
                missing_type = name;
            else
                component_fault_type = name;
            detail::VisualConfigurationHost host(
                p, p, p, Fields, refs, arrays,
                EntityAccess({M(ActorId), M(ActorWorld), M(Loaded), M(Component), {}, {}, {}}),
                [&](EntityToken t) { return t.entity == token.entity && t.generation == token.generation; },
                AnimationEvents(event_methods));
            auto binding = host.Create(token, captured_world);
            missing_type.clear();
            component_fault_type.clear();
            Check(binding && binding->Current(), "unrelated component lookup failure ended common model");
            const bool data_failed = std::string(name) == "CharacterDataComponent";
            const bool emotion_failed = std::string(name) == "PEPNFLNCDHG";
            const bool events_failed = std::string(name) == "LGBHONLHNPN";
            const auto failed_use = data_failed      ? VisualConfigurationComponent::Data
                                    : emotion_failed ? VisualConfigurationComponent::Emotion
                                                     : VisualConfigurationComponent::Events;
            Check(!Failure([&] { binding->Current(failed_use); }).empty(),
                  "unread captured identity became a request to recreate the binding");
            const ActorVisualConfigurationInput radius{false, 1.3f, {}, {}, {}, {}};
            if (data_failed)
                Check(!Failure([&] { binding->Apply(radius); }).empty(),
                      "failed Data lookup was treated as successful absence");
            else
                Check(binding->Apply(radius).unsupported_fields.empty(), "unrelated lookup blocked radius");
            if (emotion_failed)
                Check(!Failure([&] { binding->EnableEmotion(true, 0, .2f); }).empty(),
                      "failed emotion lookup was treated as successful absence");
            else
                Check(binding->EnableEmotion(true, 0, .2f), "unrelated lookup blocked emotion");
            const ActorVisualConfigurationInput events{false, {}, {}, {}, {}, std::vector<std::string>{"capture.json"}};
            if (data_failed || events_failed)
                Check(!Failure([&] { binding->Apply(events); }).empty(),
                      "event update ignored its actual Data/consumer branch dependency");
            else
                Check(binding->Apply(events).unsupported_fields.empty(), "emotion lookup failure blocked events");
            if (events_failed) {
                const auto prior_events = Read<void*>(data, 72);
                const ActorVisualConfigurationInput reset{true, {}, {}, {}, {}, {}};
                Write<void*>(data, 72, nullptr);
                Check(binding->Current(reset) && binding->Apply(reset).unsupported_fields.empty(),
                      "empty Data event override unnecessarily required the unavailable consumer");
                auto* required_events = Make("System", "String[]");
                records.at(required_events)->items.push_back(Make("System", "String", "retained.json"));
                Write(data, 72, required_events);
                Check(!Failure([&] { binding->Current(reset); }).empty(),
                      "required event reset hid its unknown consumer identity");
                const auto before_reset = writes;
                Check(!Failure([&] { binding->Apply(reset); }).empty() && writes == before_reset,
                      "required event reset wrote Data despite an unavailable consumer lookup");
                Write(data, 72, prior_events);
            }
            Check(binding->Close() && host.Close() && roots.empty(), "partial capture roots did not drain");
        }
    captured_data = nullptr;
    missing_fields_type = "CharacterDataComponent";
    {
        detail::VisualConfigurationHost host(
            p, p, p, Fields, refs, arrays,
            EntityAccess({M(ActorId), M(ActorWorld), M(Loaded), M(Component), {}, {}, {}}),
            [&](EntityToken t) { return t.entity == token.entity && t.generation == token.generation; },
            AnimationEvents(event_methods));
        auto binding = host.Create(token, captured_world);
        missing_fields_type.clear();
        Check(binding && binding->Current() && binding->EnableEmotion(true, 0, .2f),
              "unused Data overlay blocked a successful null-Data capture");
        Check(binding->Apply({false, {}, {}, {}, {}, std::vector<std::string>{"direct.json"}})
                  .unsupported_fields.empty(),
              "unused Data overlay blocked host direct-event binding");
        Check(binding->Close() && host.Close() && roots.empty(), "null-Data partial host did not drain");
    }
    captured_data = data;
    TrackedEntities().Forget(token);
}
void Capture(Provider& p, void* data, void* emotion, void* consumer) {
    const ReferenceMethods refs{M(Keep), M(Target), M(Free), {}, {}};
    const ManagedArrays arrays({M(ArrayCreate), M(ArrayLength), M(ArrayGet), M(ArraySet), {}});
    AnimationEventMethods event_methods;
    event_methods.consumer_offset = 16;
    event_methods.has_consumer = true;
    auto* model = Make("RPG.GameCore", "CharacterModelComponent");
    auto* event = Make("", "LGBHONLHNPN");
    Write(event, 16, consumer);
    captured_model = model;
    captured_data = data;
    captured_emotion = emotion;
    captured_event = event;
    const auto token = TrackedEntities().Track(reinterpret_cast<GameEntity*>(&actor));
    bool owned = true;
    detail::VisualConfigurationHost host(
        p, p, p, Fields, refs, arrays, EntityAccess({M(ActorId), M(ActorWorld), M(Loaded), M(Component), {}, {}, {}}),
        [&](EntityToken t) { return owned && t.entity == token.entity && t.generation == token.generation; },
        AnimationEvents(event_methods));
    auto binding = host.Create(token, captured_world);
    Check(binding && binding->Current(), "owned capture unavailable");
    const auto original_count = discoveries;
    Check(binding->EnableEmotion(true, 0, .2f) && discoveries == original_count, "capture repeated metadata lookup");
    const auto before = writes;
    captured_model = nullptr;
    Check(!binding->Current(), "model replacement did not invalidate capture");
    bool stale = false;
    try {
        binding->Apply({false, 1.0f, {}, {}, {}, {}});
    } catch (...) {
        stale = true;
    }
    Check(stale && writes == before, "stale capture wrote Data");
    captured_model = model;
    captured_data = nullptr;
    Check(binding->Current(), "Data replacement ended independent model binding");
    Check(!binding->Current(VisualConfigurationComponent::Data) &&
              binding->Current(VisualConfigurationComponent::Emotion),
          "component identity queries did not isolate Data replacement");
    Check(!Failure([&] { binding->Apply({false, 1.0f, {}, {}, {}, {}}); }).empty(),
          "Data replacement accepted an old component");
    Check(binding->EnableEmotion(true, 0, .2f), "Data replacement blocked independent emotion");
    captured_data = data;
    Write<void*>(event, 16, nullptr);
    Check(binding->Current(), "event replacement ended independent model binding");
    Check(!binding->Current(VisualConfigurationComponent::Events) &&
              binding->Current(VisualConfigurationComponent::Data),
          "component identity queries did not isolate event replacement");
    const auto prior_events = Read<void*>(data, 72);
    const ActorVisualConfigurationInput reset{true, {}, {}, {}, {}, {}};
    Write<void*>(data, 72, nullptr);
    Check(binding->Current(reset), "empty event reset treated an unused replacement as a dependency");
    auto* required_events = Make("System", "String[]");
    records.at(required_events)->items.push_back(Make("System", "String", "required.json"));
    Write(data, 72, required_events);
    Check(!binding->Current(reset), "nonempty event reset failed to report its replaced consumer");
    Write(data, 72, prior_events);
    binding->Apply({false, 1.0f, {}, {}, {}, {}});
    Check(!Failure([&] { binding->Apply({false, {}, {}, {}, {}, std::vector<std::string>{"event.json"}}); }).empty(),
          "event consumer replacement accepted an old consumer");
    Write(event, 16, consumer);
    captured_emotion = nullptr;
    Check(binding->Current(), "emotion replacement ended independent model binding");
    Check(!binding->Current(VisualConfigurationComponent::Emotion) &&
              binding->Current(VisualConfigurationComponent::Events),
          "component identity queries did not isolate emotion replacement");
    Check(!Failure([&] { binding->EnableEmotion(true, 0, .2f); }).empty(),
          "emotion replacement accepted an old component");
    binding->Apply({false, 1.0f, {}, {}, {}, {}});
    captured_emotion = emotion;
    owned = false;
    bool rejected = false;
    try {
        host.Create(token, captured_world);
    } catch (...) {
        rejected = true;
    }
    Check(rejected && !binding->Current(), "foreign ownership accepted");
    owned = true;
    auto* original_world = captured_world;
    captured_world = nullptr;
    Check(!binding->Current(), "world identity change ignored");
    captured_world = original_world;
    lookup_fault = true;
    Check(!binding->Current(), "failed component read accepted");
    lookup_fault = false;
    Check(binding->Close(), "captured binding did not close");
    captured_data = nullptr;
    auto story = host.Create(token, captured_world);
    Check(story && story->Current(), "capture added CharacterData readiness requirement");
    Check(story->Apply({false, 1.0f, {}, {}, {}, {}}).unsupported_fields == std::vector<std::string>{"radius"},
          "Story unsupported fields lost");
    Check(host.Close() && !story->Current() && story->Close() && host.Close() && roots.empty(),
          "capture/factory roots failed to drain");
    TrackedEntities().Forget(token);
    captured_data = data;
}
}
int main() try {
    Provider p;
    ActorVisualConfigurationFactory factory(
        p, p, p, {M(Keep), M(Target), M(Free), {}, {}},
        ManagedArrays({M(ArrayCreate), M(ArrayLength), M(ArrayGet), M(ArraySet), {}}), Fields);
    auto* data = Make("RPG.GameCore", "CharacterDataComponent");
    auto* emotion = Make("", "PEPNFLNCDHG");
    Write(data, 16, static_cast<void*>(&actor));
    Write(emotion, 16, static_cast<void*>(&actor));
    Write(data, 32, std::uint32_t{0x12345678});
    default_effects = Make("RPG.GameCore", "EffectAdaptionConfig[]");
    default_events = Make("System", "String[]");
    auto* consumer = Make("", "HHECHLOIEJO");
    bool current = true;
    const auto count = discoveries;
    Write(consumer, 112, static_cast<void*>(&info));
    Write(consumer, 176, default_events);
    auto bound = factory.Bind({{{reinterpret_cast<GameEntity*>(&actor), 1}, reinterpret_cast<GameWorld*>(&world)},
                               11,
                               data,
                               emotion,
                               [&] { return current; },
                               consumer,
                               true});
    Check(discoveries == count, "binding repeated metadata discovery");
    ActorVisualConfigurationInput empty_events;
    empty_events.event_files = std::vector<std::string>{};
    bound->Apply(empty_events);
    auto* empty_binding = Read<void*>(consumer, 176);
    Check(empty_binding && records.at(empty_binding)->items.empty(), "empty override was not bound");
    bound->Apply({true, {}, {}, {}, {}, {}});
    Check(!Read<void*>(data, 72) && Read<void*>(consumer, 176) == empty_binding,
          "ordinary reset changed its nonempty-override refresh contract");
    bound->Apply({true, {}, {}, {}, {}, std::vector<std::string>{"explicit-reset.json"}});
    Check(Read<void*>(consumer, 176) != empty_binding &&
              records.at(records.at(Read<void*>(consumer, 176))->items.at(0))->text == "explicit-reset.json",
          "explicit reset failed to rebind after an empty override");
    refresh_order.clear();
    ActorVisualConfigurationInput input;
    input.radius = 1.4f;
    input.somato = 5;
    input.effect_adaptations =
        std::vector<EffectAdaptionEntry>{{"effect.prefab", std::nullopt, "WingInA", {1, 2, 3}, {4, 5, 6}, 1}};
    input.graph_emotion = "graph.asset";
    input.event_files = std::vector<std::string>{"event.json"};
    bound->Apply(input);
    Check(Read<std::uint32_t>(data, 32) == 0x12345678 && Read<float>(data, 56) == 1.4f && Read<int>(data, 88) == 5,
          "visual patch overwrote unrelated data");
    auto* array = Read<void*>(data, 96);
    Check(array && records.at(array)->items.size() == 1 && !old_effects && new_effects == array && refreshes == 1,
          "effect adaptation refresh did not receive original override and new effective arrays");
    auto* entry = records.at(array)->items[0];
    Check(records.at(Read<void*>(entry, 16))->text == "effect.prefab" && Read<void*>(entry, 24) == nullptr &&
              Read<float>(entry, 64) == 1,
          "typed adaptation allocation lost input");
    Check(records.at(Read<void*>(data, 104))->text == "graph.asset" && bound_events == Read<void*>(data, 72) &&
              refresh_order == std::vector<std::string>{"events", "effects"},
          "atomic Graph/events data or official refresh order differs");
    Check(bound->EnableEmotion(false, 0.4f, 0.2f), "supported emotion component was reported unavailable");
    Check(transitions == 1 && blendshapes == 1 && !last_enable && last_duration == 0.4f,
          "TransitOutTime or native enum ABI differs");
    Check(bound->EnableEmotion(true, 0.4f, 0.2f), "supported emotion enable unavailable");
    Check(last_enable && last_duration == 0.2f, "TransitInTime differs");
    auto no_emotion = factory.Bind({{{reinterpret_cast<GameEntity*>(&actor), 1}, reinterpret_cast<GameWorld*>(&world)},
                                    11,
                                    data,
                                    nullptr,
                                    [&] { return current; },
                                    consumer,
                                    true});
    const auto before_emotion = transitions, before_blend = blendshapes;
    Check(!no_emotion->EnableEmotion(false, 0, 0.2f) && transitions == before_emotion && blendshapes == before_blend,
          "missing PEPN component failed or invoked an unimplemented fallback");
    Check(no_emotion->Close(), "missing-emotion binding did not close");
    bound->Apply({true, {}, {}, {}, {}, {}});
    Check(Read<float>(data, 56) == -1 && Read<int>(data, 88) == 14 && !Read<void*>(data, 96) &&
              new_effects == default_effects && !Read<void*>(data, 104) && !Read<void*>(data, 72) &&
              bound_events == default_events,
          "reset did not clear only the official visual sentinels");
    Check(Read<std::uint32_t>(data, 32) == 0x12345678, "reset touched gameplay overlay");
    ActorVisualConfigurationInput explicit_reset;
    explicit_reset.reset = true;
    explicit_reset.event_files = std::vector<std::string>{"original-source-default.json"};
    const auto data_reset = bound->Apply(explicit_reset);
    Check(data_reset.unsupported_fields.empty() && !Read<void*>(data, 72) && !Read<void*>(data, 104) &&
              Read<float>(data, 56) == -1 && Read<int>(data, 88) == 14 &&
              records.at(records.at(bound_events)->items.at(0))->text == "original-source-default.json",
          "data-present explicit reset did not preserve defaults and bind authored event list");
    auto absent = factory.Bind({{{reinterpret_cast<GameEntity*>(&actor), 1}, reinterpret_cast<GameWorld*>(&world)},
                                11,
                                data,
                                emotion,
                                [&] { return current; },
                                nullptr,
                                false});
    const auto before_absent = writes, refresh_before_absent = refreshes;
    const auto events_before_absent = bound_events;
    absent->Apply(input);
    Check(writes > before_absent && Read<float>(data, 56) == 1.4f && refreshes == refresh_before_absent + 1 &&
              bound_events == events_before_absent,
          "absent event component delayed Data or suppressed effect refresh");
    Check(absent->Close(), "absent-component binding did not close");
    auto missing = factory.Bind({{{reinterpret_cast<GameEntity*>(&actor), 1}, reinterpret_cast<GameWorld*>(&world)},
                                 11,
                                 data,
                                 emotion,
                                 [&] { return current; },
                                 nullptr,
                                 true});
    auto changed = input;
    changed.radius = 1.8f;
    changed.graph_emotion = "new-graph.asset";
    changed.event_files = std::vector<std::string>{"new-event.json"};
    const auto before_missing = writes, refresh_before_missing = refreshes;
    bool missing_failed = false;
    std::string missing_error;
    try {
        missing->Apply(changed);
    } catch (const std::exception& e) {
        missing_failed = true;
        missing_error = e.what();
    }
    Check(missing_failed && writes > before_missing && Read<float>(data, 56) == 1.8f &&
              records.at(Read<void*>(data, 104))->text == "new-graph.asset" && refreshes == refresh_before_missing,
          "incomplete existing event component lost partial Data write or continued after failure");
    Check(missing_error.find("configuration already changed") != std::string::npos,
          "partial-write failure hid committed Data");
    Check(missing->Close(), "missing-consumer binding did not close");
    fail_blend = true;
    bool failed = false;
    try {
        bound->EnableEmotion(false, 0, 0);
    } catch (...) {
        failed = true;
    }
    fail_blend = false;
    Check(failed && transitions == 3, "partial native failure was retried or suppressed");
    const auto before = writes;
    current = false;
    failed = false;
    try {
        bound->Apply(input);
    } catch (...) {
        failed = true;
    }
    Check(failed && writes == before, "stale model wrote configuration");
    current = true;
    auto* original_paths = Read<void*>(consumer, 176);
    auto* original_layers = Make("System", "String[]");
    Write(consumer, 128, original_layers);
    auto story = factory.Bind({{{reinterpret_cast<GameEntity*>(&actor), 1}, reinterpret_cast<GameWorld*>(&world)},
                               11,
                               nullptr,
                               emotion,
                               [&] { return current; },
                               consumer,
                               true});
    const auto before_story_writes = writes;
    const auto partial = story->Apply(input);
    Check(partial.unsupported_fields ==
                  std::vector<std::string>{"radius", "somato", "effect_adaptations", "graph_emotion"} &&
              writes == before_story_writes,
          "data-null actor lost exact supported/unsupported fields");
    auto* story_paths = Read<void*>(consumer, 176);
    Check(story_paths != original_paths && records.at(story_paths)->items.size() == 1 &&
              Read<void*>(consumer, 128) == original_layers,
          "independent story event input was not adopted");
    Check(story->EnableEmotion(true, 0, 0.2f), "data-null actor emotion was unnecessarily blocked");
    const auto restored = story->Apply({true, {}, {}, {}, {}, {}});
    Check(restored.unsupported_fields.size() == 4 && Read<void*>(consumer, 176) == original_paths &&
              Read<void*>(consumer, 128) == original_layers,
          "data-null reset did not restore exact actor event baseline");
    const auto explicit_story_reset = story->Apply(explicit_reset);
    Check(explicit_story_reset.unsupported_fields.size() == 4 && Read<void*>(consumer, 176) != original_paths &&
              records.at(records.at(Read<void*>(consumer, 176))->items.at(0))->text == "original-source-default.json" &&
              Read<void*>(consumer, 128) == original_layers,
          "data-null explicit reset reused stale post-swap binding baseline");
    Write<void*>(consumer, 112, nullptr);
    bool not_ready = false;
    try {
        story->Apply(input);
    } catch (const std::exception& e) {
        not_ready = std::string(e.what()).find("not ready") != std::string::npos;
    }
    Check(not_ready, "unready event consumer falsely reported binding success");
    Write(consumer, 112, static_cast<void*>(&info));
    Check(story->Close(), "story visual binding roots did not drain");
    Check(bound->Close() && factory.Close() && roots.empty(), "successful native roots did not drain");
    {
        ActorVisualConfigurationFactory bad(
            p, p, p, {M(Keep), M(Target), M(Free), {}, {}},
            ManagedArrays({M(ArrayCreate), M(ArrayLength), M(ArrayGet), M(ArraySet), {}}),
            [](const char* ns, const char* name) {
                auto fields = Fields(ns, name);
                if (std::string(name) == "IJDDNGPOEHK")
                    fields.front().offset += 4;
                return fields;
            });
        auto partial_binding = bad.Bind({{{reinterpret_cast<GameEntity*>(&actor), 1}, reinterpret_cast<GameWorld*>(&world)},
                                         11, data, emotion, [] { return true; }, consumer, true});
        const auto before_bad_layout = writes;
        Check(Failure([&] { partial_binding->Apply({false, 1.0f, {}, {}, {}, {}}); }).find("radius") != std::string::npos &&
                  writes == before_bad_layout,
              "unqualified radius layout wrote the overlay");
        Check(partial_binding->EnableEmotion(true, 0, .2f), "unrelated radius layout blocked emotion");
        Check(partial_binding->Close() && bad.Close() && roots.empty(), "partial-layout roots did not drain");
    }
    TestIndependentBindings(p, data, emotion, consumer);
    Capture(p, data, emotion, consumer);
    TestIndependentCapture(p, data, emotion, consumer);
    // A failed GC decrement has unknown outcome. Preserve its receipt and
    // reject new work; never issue a second decrement to manufacture success.
    ActorVisualConfigurationFactory fault_factory(
        p, p, p, {M(Keep), M(Target), M(Free), {}, {}},
        ManagedArrays({M(ArrayCreate), M(ArrayLength), M(ArrayGet), M(ArraySet), {}}), Fields);
    auto fault = fault_factory.Bind({{{reinterpret_cast<GameEntity*>(&actor), 1}, reinterpret_cast<GameWorld*>(&world)},
                                     11,
                                     data,
                                     emotion,
                                     [&] { return current; },
                                     consumer,
                                     true});
    fail_free_once = true;
    Check(!fault->Close() && !fault->Current(), "partially closed binding accepted new commands");
    const auto frees = free_calls;
    Check(!fault->Close() && free_calls == frees, "unknown GC decrement was retried");
    Check(!fault_factory.Close() && roots.size() == 1, "unknown GC receipt was discarded");
    const auto final_frees = free_calls;
    Check(!fault_factory.Close() && free_calls == final_frees, "factory retried an unknown decrement");
    std::cout << "Actor visual configuration native provider: subset, default reset, adaptation, emotion fades, epoch "
                 "and GC receipts passed\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
}
