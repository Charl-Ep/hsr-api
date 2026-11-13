#include "../../src/model/ActorModelChangeNative.hpp"
#include <hksr/model/ModelAccess.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <Windows.h>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace hksr::api;
namespace {
void Check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void Reject(F operation) {
    bool rejected = false;
    try {
        operation();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected, "foreign actor binding accepted");
}
struct Model {
    unsigned changes = 0;
    bool animator_initialized = false;
    void* art = nullptr;
};
struct Entity {
    GameWorld* world = nullptr;
    Model* model = nullptr;
};
int info = 0, next_root = 0;
std::map<int, void*> roots;
unsigned can_calls = 0, change_calls = 0;
bool can_result = true, change_result = true, can_fault = false, change_fault = false;
bool saw_null_resolve = false, saw_null_change = false;
std::function<void()> resolving, changing;
std::string default_path = "official/default.prefab";
template <class F> Method M(F fn) {
    return {reinterpret_cast<void*>(fn), &info};
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
GameWorld* __fastcall World(Entity* entity, void*) {
    return entity->world;
}
std::uint32_t __fastcall Id(Entity*, void*) {
    return 9;
}
bool __fastcall Loaded(Entity*, void*) {
    return true;
}
void* __fastcall Component(Entity* entity, std::string* type, void*) {
    Check(*type == "RPG.GameCore.CharacterModelComponent", "wrong component type");
    return entity->model;
}
__declspec(noinline) bool __fastcall Can(Model*, std::string* path, void** resolved, void*) {
    ++can_calls;
    saw_null_resolve = !path;
    if (resolving)
        resolving();
    if (can_fault)
        RaiseException(0xe0677701, 0, 0, nullptr);
    *resolved = path ? path : &default_path;
    return can_result;
}
__declspec(noinline) bool __fastcall Change(Model* model, std::string* path, void* prepared, void*) {
    ++change_calls;
    saw_null_change = !path;
    Check(!prepared, "prepared root was not null");
    if (change_result) {
        ++model->changes;
        model->art = reinterpret_cast<void*>(static_cast<std::uintptr_t>(model->changes));
    }
    if (changing)
        changing();
    if (change_fault)
        RaiseException(0xe0677702, 0, 0, nullptr);
    return change_result;
}
struct Provider : MethodResolver, ManagedObjects {
    unsigned resolves = 0, types = 0;
    std::vector<std::unique_ptr<std::string>> texts;
    Method Resolve(const char* ns, const char* owner, const char* name, const char* result,
                   std::initializer_list<const char*> params, MethodKind kind) override {
        ++resolves;
        Check(std::string(ns) == "RPG.GameCore" && std::string(owner) == "CharacterModelComponent" &&
                  std::string(result) == "System.Boolean" && params.size() == 2 && kind == MethodKind::Instance,
              "unexpected metadata preparation");
        if (std::string(name) == "CanChangeModel")
            return M(Can);
        Check(std::string(name) == "ChangeModel", "unexpected leaf method");
        return M(Change);
    }
    ObjectResult ManagedType(const char* ns, const char* type) override {
        ++types;
        texts.push_back(std::make_unique<std::string>(std::string(ns) + "." + type));
        return {{true, 0}, texts.back().get()};
    }
    ObjectResult NewStringUtf8(const char* value) override {
        texts.push_back(std::make_unique<std::string>(value));
        return {{true, 0}, texts.back().get()};
    }
    ObjectResult AllocateObject(const char*, const char*) override {
        throw std::runtime_error("unexpected operation/loader allocation");
    }
};
}
int main() try {
    Provider provider;
    int world_value = 0;
    Model model, replacement;
    Entity entity{reinterpret_cast<GameWorld*>(&world_value), &model};
    const auto token = TrackedEntities().Track(reinterpret_cast<GameEntity*>(&entity));
    EntityAccess entities({M(Id), M(World), M(Loaded), M(Component), {}, {}, {}});
    ModelAccess models(ModelAccessMethods{});
    detail::ActorModelChangeFactory factory(provider, provider, {M(Keep), M(Target), M(Free), {}, {}}, entities, models,
                                            EntityOwner{}, [&](auto t, auto* w) {
                                                return t.entity == token.entity && t.generation == token.generation &&
                                                       w == entity.world;
                                            });
    const auto prepared = provider.resolves;
    Check(prepared == 2 && provider.types == 1, "leaf metadata not prepared once");
    auto request = factory.Create(token, entity.world, std::nullopt);
    request->Service();
    Check(request->Status().phase == ActorModelChangePhase::Ready && request->Status().submitted &&
              request->Status().may_have_changed && saw_null_resolve && saw_null_change,
          "official nullable default resolution/change path not used");
    Check(!model.animator_initialized && model.changes == 1,
          "success invented Animator initialization or extra adoption");
    request->Service();
    Check(can_calls == 1 && change_calls == 1, "terminal request submitted more than once");
    Check(!request->Cancel() && request->Close(), "submitted change was cancelled/restored");
    auto cancelled = factory.Create(token, entity.world, "another.prefab");
    Check(cancelled->Cancel(), "pending cancellation rejected");
    cancelled->Service();
    Check(change_calls == 1 && cancelled->Status().phase == ActorModelChangePhase::Cancelled && cancelled->Close(),
          "cancelled pending request wrote model");
    can_result = false;
    auto unchanged = factory.Create(token, entity.world, "declined.prefab");
    unchanged->Service();
    Check(unchanged->Status().phase == ActorModelChangePhase::Unchanged && !unchanged->Status().submitted &&
              change_calls == 1 && unchanged->Close(),
          "CanChange false ran model change or became success");
    can_result = true;
    change_result = false;
    auto declined = factory.Create(token, entity.world, "raced.prefab");
    declined->Service();
    Check(declined->Status().phase == ActorModelChangePhase::Unchanged && declined->Status().submitted &&
              !declined->Status().may_have_changed && declined->Close(),
          "Change false ran success branch");
    change_result = true;
    auto reentry = factory.Create(token, entity.world, "cancel-resolving.prefab");
    const auto before = change_calls;
    resolving = [&] {
        Check(reentry->Cancel(), "resolution cancellation rejected");
        reentry->Service();
    };
    reentry->Service();
    resolving = {};
    Check(reentry->Status().phase == ActorModelChangePhase::Cancelled && change_calls == before && reentry->Close(),
          "cancel during resolution or reentrant Service submitted");
    change_fault = true;
    auto partial = factory.Create(token, entity.world, "partial.prefab");
    partial->Service();
    change_fault = false;
    Check(partial->Status().phase == ActorModelChangePhase::Failed && partial->Status().submitted &&
              partial->Status().may_have_changed && !partial->Status().change.call.returned && partial->Close(),
          "partial native failure became success or auto-restored");
    can_fault = true;
    auto resolution_fault = factory.Create(token, entity.world, "resolve-error.prefab");
    const auto before_fault = change_calls;
    resolution_fault->Service();
    can_fault = false;
    Check(resolution_fault->Status().phase == ActorModelChangePhase::Failed && !resolution_fault->Status().submitted &&
              change_calls == before_fault && resolution_fault->Close(),
          "failed resolution still submitted");
    auto stale = factory.Create(token, entity.world, "stale.prefab");
    entity.model = &replacement;
    const auto before_stale = can_calls;
    stale->Service();
    Check(stale->Status().phase == ActorModelChangePhase::Failed && can_calls == before_stale && stale->Close(),
          "replaced model component received old request");
    entity.model = &model;
    auto ended = factory.Create(token, entity.world, "ended.prefab");
    changing = [&] { entity.model = &replacement; };
    ended->Service();
    changing = {};
    Check(ended->Status().phase == ActorModelChangePhase::Failed && ended->Status().change.changed &&
              ended->Status().may_have_changed && ended->Close(),
          "native true after owner/component replacement became success");
    entity.model = &model;
    auto closed = factory.Create(token, entity.world, "closing.prefab");
    changing = [&] { Check(!closed->Cancel() && !closed->Close(), "in-call close released active input roots"); };
    closed->Service();
    changing = {};
    Check(closed->Close(), "in-call close did not drain after return");
    for (int i = 0; i < 100; ++i) {
        auto repeat = factory.Create(token, entity.world, "repeat.prefab");
        repeat->Service();
        Check(repeat->Status().phase == ActorModelChangePhase::Ready && repeat->Close(), "repeat request failed");
    }
    Check(provider.resolves == prepared && provider.types == 1 && roots.size() == 1,
          "per-request metadata discovery or GC growth");
    Entity foreign = entity;
    const auto other = TrackedEntities().Track(reinterpret_cast<GameEntity*>(&foreign));
    Reject([&] { factory.Create(other, foreign.world, "foreign.prefab"); });
    TrackedEntities().Forget(other);
    Check(factory.Close() && roots.empty(), "factory roots did not drain");
    TrackedEntities().Forget(token);
    std::cout << "Actor model change production provider: sync leaf, nullable default, exact success, cancellation and "
                 "partial failure passed\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
}
