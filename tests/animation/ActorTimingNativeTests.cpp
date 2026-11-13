#include "../../src/animation/ActorTimingNative.hpp"
#include <hksr/model/ModelAccess.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <map>
#include <stdexcept>
#include <Windows.h>
using namespace hksr::api;
namespace {
void Check(bool ok) {
    if (!ok)
        throw std::runtime_error("actor timing native provider");
}
struct Animator {
    float weight = .25f;
    int controller = 1;
};
struct Model {
    int art = 0;
    Animator animator;
    bool changing = false;
};
struct Animation {
    float speed = -1;
    Model* model = nullptr;
};
struct Stack {
    int next = 0, removes = 0;
    std::map<int, bool> entries;
    float factor = 0, duration = 0, fade_in = 0, fade_out = 0, delay = 0;
};
struct Entity {
    Stack* stack = nullptr;
    GameWorld* world = nullptr;
};
Model model;
Animation battle{-1, &model}, adventure{-1, &model};
bool battle_active = true, getter_fault = false;
int info = 0, root_next = 0;
unsigned writes = 0;
float tween_duration = 0;
std::map<int, void*> roots;
std::vector<std::unique_ptr<std::string>> strings;
void* fail_keep_object = nullptr;
void* fail_free_object = nullptr;
unsigned failed_free_calls = 0;
template <class F> Method M(F f) {
    return {reinterpret_cast<void*>(f), &info};
}
int __fastcall Keep(void* object, int, int, void*) {
    if (object == fail_keep_object)
        return 0;
    roots[++root_next] = object;
    return root_next;
}
void* __fastcall Root(int id, void*) {
    return roots.at(id);
}
void __fastcall Free(int id, void*) {
    if (roots.at(id) == fail_free_object) {
        ++failed_free_calls;
        RaiseException(0xe0001235, 0, 0, nullptr);
    }
    roots.erase(id);
}
GameWorld* __fastcall World(Entity* self, void*) {
    return self->world;
}
std::uint32_t __fastcall RuntimeId(Entity*, void*) {
    return 7;
}
bool __fastcall Loaded(Entity*, void*) {
    return true;
}
void* __fastcall Component(Entity*, std::string* type, void*) {
    if (*type == "RPG.GameCore.CharacterModelComponent")
        return &model;
    if (*type == ".CFFPEAALCPH")
        return battle_active ? &battle : nullptr;
    Check(*type == ".OKMFNNDDCGA");
    return &adventure;
}
void* __fastcall Art(Model* self, void*) {
    return &self->art;
}
void* __fastcall AnimatorOf(Model* self, void*) {
    return &self->animator;
}
bool __fastcall Changing(Model* self, void*) {
    return self->changing;
}
void* __fastcall UnityAnimator(Animation* self, void*) {
    if (getter_fault)
        RaiseException(0xe0001234, 0, 0, nullptr);
    return &self->model->animator;
}
void* __fastcall Mock(Animation* self, void*) {
    return self;
}
void* __fastcall Controller(Animator* self, void*) {
    return &self->controller;
}
int __fastcall LayerIndex(Animator*, std::string* name, void*) {
    return *name == "Hit" ? 1 : -1;
}
float __fastcall LayerWeight(Animator* self, int layer, void*) {
    Check(layer == 1);
    return self->weight;
}
void __fastcall Tween(Animation* self, std::string* name, float weight, float duration, void*) {
    Check(*name == "Hit");
    ++writes;
    self->model->animator.weight = weight;
    tween_duration = duration;
}
int __fastcall Infinite(Stack* self, float factor, std::string*, float fade_in, float fade_out, void* a, void* b,
                        float delay, bool flag, void*) {
    Check(!a && !b && !flag);
    self->factor = factor;
    self->fade_in = fade_in;
    self->fade_out = fade_out;
    self->delay = delay;
    const auto id = ++self->next;
    self->entries[id] = true;
    return id;
}
int __fastcall Finite(Stack* self, float factor, float duration, std::string* key, float fade_in, float fade_out,
                      void* a, void* b, float delay, bool flag, void* metadata) {
    self->duration = duration;
    return Infinite(self, factor, key, fade_in, fade_out, a, b, delay, flag, metadata);
}
bool __fastcall Contains(Stack* self, int id, void*) {
    return self->entries[id];
}
void __fastcall Remove(Stack* self, int id, bool fade, void*) {
    ++self->removes;
    if (!fade)
        self->entries[id] = false;
}
struct Provider : MethodResolver, ManagedObjects {
    int resolves = 0, type_reads = 0, field_reads = 0;
    std::string missing_type, missing_method, missing_field;
    std::map<std::string, std::unique_ptr<std::string>> types;
    ObjectResult ManagedType(const char* ns, const char* name) override {
        ++type_reads;
        if (name == missing_type)
            return {{true, 0}, nullptr};
        const auto key = std::string(ns) + "." + name;
        auto& item = types[key];
        if (!item)
            item = std::make_unique<std::string>(key);
        return {{true, 0}, item.get()};
    }
    ObjectResult NewStringUtf8(const char* text) override {
        strings.push_back(std::make_unique<std::string>(text));
        return {{true, 0}, strings.back().get()};
    }
    ObjectResult AllocateObject(const char*, const char*) override {
        throw std::runtime_error("unexpected allocation");
    }
    Method Resolve(const char*, const char*, const char* name, const char*, std::initializer_list<const char*>,
                   MethodKind) override {
        ++resolves;
        const std::string key = name;
        if (key == missing_method)
            throw std::runtime_error("synthetic method absent: " + key);
        if (key == "GetUnityAnimator" || key == "get_Animator")
            return M(UnityAnimator);
        if (key == "FBHJPDAEDGI")
            return M(Mock);
        if (key == "get_runtimeAnimatorController")
            return M(Controller);
        if (key == "GetLayerIndex")
            return M(LayerIndex);
        if (key == "GetLayerWeight")
            return M(LayerWeight);
        if (key == "AGLJAHOBLIO")
            return M(Tween);
        if (key == "ScaleInfinitely")
            return M(Infinite);
        if (key == "ScaleByDuration")
            return M(Finite);
        if (key == "RemoveScaleByIndex")
            return M(Remove);
        Check(key == "ContainsIndex");
        return M(Contains);
    }
    std::vector<DeclaredField> Fields(const char*, const char* owner) {
        ++field_reads;
        if (owner == missing_field)
            return {};
        if (std::string(owner) == "GameEntity")
            return {{"RPG.GameCore", "GameEntity", "TimeScaleStack", "RPG.GameCore.TimeScaleStack",
                     offsetof(Entity, stack), 6, true}};
        return {{"", "CFFPEAALCPH", "GIFCMBFPFMB", "System.Single", offsetof(Animation, speed), 6, true}};
    }
};
}
void TestActorTimingNative() {
    Provider provider;
    Stack stack, replacement;
    int world = 0;
    Entity entity{&stack, reinterpret_cast<GameWorld*>(&world)};
    const auto token = TrackedEntities().Track(reinterpret_cast<GameEntity*>(&entity));
    EntityAccess entities({M(RuntimeId), M(World), M(Loaded), M(Component), {}, {}, {}});
    ModelAccess models({M(Art), M(AnimatorOf), {}, M(Changing), {}, {}});
    ReferenceMethods refs{M(Keep), M(Root), M(Free), {}, {}};
    detail::ActorTimingFactory factory(
        provider, provider, [&](const auto* ns, const auto* name) { return provider.Fields(ns, name); }, refs, entities,
        models, EntityOwner{},
        [&](EntityToken t, GameWorld* w) {
            return t.entity == token.entity && t.generation == token.generation && w == entity.world;
        });
    const auto prepared = provider.resolves;
    Check(prepared > 0 && provider.type_reads == 3 && provider.field_reads == 2);
    const auto prepared_roots = roots.size();
    fail_keep_object = entity.world;
    bool failed_creation = false;
    try {
        factory.Create(token, entity.world);
    } catch (const std::runtime_error&) {
        failed_creation = true;
    }
    Check(failed_creation && roots.size() == prepared_roots);
    fail_free_object = &entity;
    std::string cleanup_error;
    try {
        factory.Create(token, entity.world);
    } catch (const std::runtime_error& error) {
        cleanup_error = error.what();
    }
    Check(cleanup_error.find("actor timing retention failed") != std::string::npos &&
          cleanup_error.find("creation input cleanup unresolved") != std::string::npos && failed_free_calls == 1 &&
          roots.size() == prepared_roots + 1);
    fail_keep_object = fail_free_object = nullptr;
    // Discard only the substitute handle whose decrement outcome is unknown.
    // The SDK must not issue another decrement to manufacture cleanup success.
    for (auto it = roots.begin(); it != roots.end();)
        if (it->second == &entity)
            it = roots.erase(it);
        else
            ++it;
    auto api = factory.Create(token, entity.world);
    auto second = factory.Create(token, entity.world);
    auto layer = api->Layer("Hit");
    Check(layer && layer->value == .25f);
    Check(api->SetLayer(layer->model, "Hit", .8f, .6f) && model.animator.weight == .8f && tween_duration == .6f);
    auto speed = api->Speed();
    Check(speed && speed->value == -1);
    Check(api->SetSpeed(speed->model, .4f) && battle.speed == .4f);
    Check(api->SetSpeed(speed->model, -1) && battle.speed == -1);
    ActorTimeScaleInput input;
    input.factor = .3f;
    input.duration = 2.0f;
    input.fade_in = .2f;
    input.fade_out = .4f;
    input.delay = .1f;
    const auto clock = api->Scale(input, "finite");
    Check(clock.value && stack.factor == .3f && stack.duration == 2 && stack.fade_in == .2f && stack.fade_out == .4f &&
          stack.delay == .1f);
    Check(api->Remove(clock, true) && *api->Contains(clock));
    Check(api->Remove(clock, false) && !*api->Contains(clock));
    const auto retained = roots.size();
    for (int i = 0; i < 4; ++i) {
        const auto temporary = api->Scale(input, "temporary" + std::to_string(i));
        Check(api->Remove(temporary, false));
    }
    Check(roots.size() == retained); // completed clock keys are not a growing per-actor text cache
    input.duration.reset();
    const auto old = api->Scale(input, "old-stack");
    entity.stack = &replacement;
    const auto before = stack.removes;
    Check(!*api->Contains(old) && api->Remove(old, false) && stack.removes == before);
    model.changing = true;
    Check(!api->Layer("Hit") && !api->SetSpeed(speed->model, .7f));
    model.changing = false;
    getter_fault = true;
    bool failed = false;
    try {
        api->Layer("Hit");
    } catch (const std::runtime_error&) {
        failed = true;
    }
    getter_fault = false;
    Check(failed); // a failed native getter is not successful absence/Skipped
    battle_active = false;
    auto exploration = second->Layer("Hit");
    Check(exploration && second->SetLayer(exploration->model, "Hit", .2f, .1f) && !second->Speed());
    Check(provider.resolves == prepared && provider.type_reads == 3 &&
          provider.field_reads == 2); // no per-actor/operation discovery
    const auto first_clock = api->Scale(input, "source-a"), second_clock = second->Scale(input, "source-b");
    Check(*api->Contains(first_clock) && *second->Contains(second_clock));
    model.changing = true;
    Check(*second->Contains(second_clock));
    model.changing = false; // actor clock survives receiver model handoff
    Check(api->Remove(first_clock, false) && *second->Contains(second_clock));
    // Independent adaptation failures must not suppress the other receiver or actor clock.
    auto partial = [&](const std::string& type, const std::string& method, const std::string& field,
                       bool battle_receiver, bool expect_layer, bool expect_clock) {
        Provider missing;
        missing.missing_type = type;
        missing.missing_method = method;
        missing.missing_field = field;
        battle_active = battle_receiver;
        detail::ActorTimingFactory partial_factory(
            missing, missing, [&](const auto* ns, const auto* name) { return missing.Fields(ns, name); }, refs,
            entities, models, EntityOwner{},
            [&](EntityToken t, GameWorld* w) {
                return t.entity == token.entity && t.generation == token.generation && w == entity.world;
            });
        auto independent = partial_factory.Create(token, entity.world);
        bool layer_ok = false;
        try {
            layer_ok = independent->Layer("Hit").has_value();
        } catch (const std::runtime_error&) {
        }
        Check(layer_ok == expect_layer);
        if (method == "GetLayerIndex")
            Check(independent->Speed().has_value());
        bool clock_ok = false;
        try {
            const auto clock = independent->Scale(input, "partial");
            clock_ok = clock.value != 0;
            if (clock.value)
                Check(independent->Remove(clock, false));
        } catch (const std::runtime_error&) {
        }
        Check(clock_ok == expect_clock);
        if (expect_layer && method == "AGLJAHOBLIO") {
            const auto observed = independent->Layer("Hit");
            bool rejected = false;
            try {
                independent->SetLayer(observed->model, "Hit", .5f, 0);
            } catch (const std::runtime_error&) {
                rejected = true;
            }
            Check(rejected);
        }
        Check(independent->Close() && partial_factory.Close());
    };
    partial("CFFPEAALCPH", "", "", false, true, true);
    partial("OKMFNNDDCGA", "", "", true, true, true);
    partial("", "RemoveScaleByIndex", "", true, true, false);
    partial("", "", "CFFPEAALCPH", true, true, true);
    partial("", "ScaleByDuration", "", true, true, true); // infinite start does not require finite start
    partial("", "AGLJAHOBLIO", "", true, true, true);     // read is not coupled to layer tween
    partial("", "GetLayerIndex", "", true, false, true);  // speed is not coupled to layer observation
    partial("CharacterModelComponent", "", "", true, false, true);
    battle_active = false;
    const auto receiver_removes = replacement.removes;
    TrackedEntities().Disposed(token);
    Check(!*second->Contains(second_clock) && second->Remove(second_clock, false) &&
          replacement.removes == receiver_removes);
    Check(api->Close() && second->Close() && factory.Close() && roots.empty());
    Check(failed_free_calls == 1);
    TrackedEntities().Forget(token);
}
