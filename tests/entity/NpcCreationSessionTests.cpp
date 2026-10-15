#include <hksr/entity/NpcCreationSession.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#include <hksr/presentation/PresentationContext.hpp>
#include <hksr/presentation/PresentationBindings.hpp>
#include <Windows.h>
#include <iostream>
#include <map>
#include <stdexcept>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<hksr::api::NpcCreationSession>);
static_assert(!std::is_copy_assignable_v<hksr::api::NpcCreationSession>);

namespace {
using namespace hksr::api;
struct FakeEntity { bool loaded = false; std::uint32_t id = 42; } objects[2];
int world_data, manager_data, definition_data, row_data, metadata, type_data;
GameWorld* world = reinterpret_cast<GameWorld*>(&world_data);
auto* manager = reinterpret_cast<EntityManager*>(&manager_data);
unsigned creations = 0, kills = 0, reads = 0, setter_calls = 0;
unsigned factory_mode = 0;
bool kill_fault = false, getter_fault = false, auto_dispose = true;
bool cascade = false;
EntityToken factory_token;
EntityOwner* current_owner = nullptr;
EntityOwner* unrelated_owner = nullptr;
int next_handle = 0;
std::map<int, void*> roots;
void Check(bool ok) { if (!ok) throw std::runtime_error("NPC instance test failed"); }
GameEntity* E(unsigned n = 0) { return reinterpret_cast<GameEntity*>(&objects[n]); }
template<class F> Method M(F fn) { return {reinterpret_cast<void*>(fn), &metadata}; }
__declspec(noinline) GameEntity* __fastcall Empty(GameWorld* w, int team, int kind, void* info) {
    Check(w == world && team == 4 && kind == 4 && info == &metadata);
    ++creations; return E();
}
__declspec(noinline) void __fastcall Dispose(GameEntity*, void* info) { Check(info == &metadata); }
GameEntity* __fastcall Create(EntityManager* m, MapNpcDefinition* definition, NpcDataRow* row,
    EntityCallback* callback, bool option, void* info) {
    Check(m == manager && definition == reinterpret_cast<MapNpcDefinition*>(&definition_data) &&
        row == reinterpret_cast<NpcDataRow*>(&row_data) && !callback && option && info == &metadata);
    Check(roots.size() == 4); // all inputs retained before entering the official call
    auto* entity = Empty(world, 4, 4, &metadata);
    Check(current_owner->Owns(entity) && !unrelated_owner->Owns(entity)); // before outer factory return/registration
    factory_token=current_owner->OwnedToken(entity);
    Check(factory_token.entity==entity && factory_token.generation!=0);
    if (factory_mode == 2) Dispose(entity, &metadata);
    if (factory_mode == 3) EntityCreationScope::FactoryCompleted(world, E(1));
    if (factory_mode) RaiseException(0xE0510001, 0, 0, nullptr);
    return entity;
}
GameEntity* __fastcall ReturnExisting(EntityManager*, MapNpcDefinition*, NpcDataRow*, EntityCallback*, bool, void*) {
    return E();
}
std::uint32_t __fastcall Id(GameEntity* e, void* info) { Check(info == &metadata); ++reads; return reinterpret_cast<FakeEntity*>(e)->id; }
GameWorld* __fastcall Owner(GameEntity*, void*) { return world; }
bool __fastcall Loaded(GameEntity* e, void*) {
    if (getter_fault) RaiseException(0xE0510002, 0, 0, nullptr);
    return reinterpret_cast<FakeEntity*>(e)->loaded;
}
void* __fastcall Component(GameEntity* e, void* type, void* info) { Check(e == E() && type == &type_data && info == &metadata); return &type_data; }
GameEntity* __fastcall Lookup(EntityManager* m, std::uint32_t id, void* info) { Check(m == manager && id == 42 && info == &metadata); return E(); }
void __fastcall Immediate(GameEntity*, bool value, void* info) { Check(value && info == &metadata); ++setter_calls; }
void __fastcall Kill(GameWorld* w, GameEntity* e, GameEntity* source, void* info) {
    Check(w == world && !source && info == &metadata); ++kills;
    if (kill_fault) RaiseException(0xE0510003, 0, 0, nullptr);
    if (auto_dispose) Dispose(e, &metadata);
    if (cascade && e == E()) Dispose(E(1), &metadata);
}
int __fastcall Retain(void* value, int zero, int kind, void*) { Check(!zero && kind == 2); roots[++next_handle] = value; return next_handle; }
void* __fastcall Target(int handle, void*) { return roots.at(handle); }
void __fastcall Free(int handle, void*) { roots.erase(handle); }
MetadataResolver Resolver() {
    return MetadataResolver({
        {"RPG.GameCore", "GameEntity", {
            {"Dispose", "System.Void", {}, false, M(Dispose)},
            {"get_RuntimeID", "System.UInt32", {}, false, M(Id)},
            {"get_OwnerWorldRef", "RPG.GameCore.GameWorld", {}, false, M(Owner)},
            {"get_IsLoaded", "System.Boolean", {}, false, M(Loaded)},
            {"GetComponent", "RPG.GameCore.GameComponentBase", {"System.Type"}, false, M(Component)},
            {"set_KillImmediately", "System.Void", {"System.Boolean"}, false, M(Immediate)}}},
        {"RPG.GameCore", "GameWorld", {
            {"CreateEmptyEntity", "RPG.GameCore.GameEntity", {"RPG.GameCore.TeamType", "RPG.GameCore.EntityType"}, false, M(Empty)},
            {"MakeEntityDie", "System.Void", {"RPG.GameCore.GameEntity", "RPG.GameCore.GameEntity"}, false, M(Kill)}}},
        {"RPG.GameCore", "EntityManager", {{"GetEntityByRuntimeID", "RPG.GameCore.GameEntity", {"System.UInt32"}, false, M(Lookup)}}},
        {"RPG.GameCore", "EntityManagerExtension", {{"ClientCreateMapNPC", "RPG.GameCore.GameEntity",
            {"RPG.GameCore.EntityManager", "RPG.Client.MapNpcDef", "RPG.GameCore.NPCDataRow", "System.Action<RPG.GameCore.GameEntity>", "System.Boolean"}, true, M(Create)}}}
    });
}
}
void TestNpcCreationSessions() {
    EntityOwner owner, unrelated;
    current_owner = &owner; unrelated_owner = &unrelated;
    auto resolver = Resolver();
    const auto methods = ResolveEntityAccessMethods(resolver);
    Check(methods.binding_errors.empty());
    const auto factory = ResolveMapNpcFactory(resolver);
    const ReferenceMethods references{M(Retain), M(Target), M(Free), {}, {}};
    NpcCreateInput input{world, manager, reinterpret_cast<MapNpcDefinition*>(&definition_data), reinterpret_cast<NpcDataRow*>(&row_data), true};
    WorldFrame frame; frame.world = world; frame.disposing = {false, {true, 0}};
    InstallEntityLifecycleHooks(resolver);
    {
        EntityAccess access(methods);
        Check(access.Component(E(), &type_data).object == &type_data);
        Check(access.Lookup(manager, 42).value == E());
        std::vector<EntityOrigin> origins;
        NpcCreationSession instance(owner, factory, methods, references,[&](EntityOrigin value){origins.push_back(value);});
        instance.Create(input); Check(instance.Status().create_returned && roots.size() == 5);
        Check(origins.size()==2 && origins[0].token.generation==factory_token.generation &&
            origins[1].token.generation==factory_token.generation && origins[0].world==world);
        Check(current_owner->Owns(E()) && !current_owner->Owns(E(1)) && !unrelated.Owns(E()));
        Check(instance.Status().entities[0].token.generation==factory_token.generation);
        Check(current_owner->OwnedToken(E()).generation==factory_token.generation);
        instance.Service(frame); Check(!instance.Status().entities[0].observation.loaded.value);
        objects[0].loaded = true; instance.Service(frame);
        Check(instance.Status().entities[0].observation.loaded.value);
        getter_fault = true; instance.Service(frame); getter_fault = false;
        Check(!instance.Status().entities[0].observation.loaded.read.returned);
        instance.Remove(); auto wrong = frame; wrong.world = reinterpret_cast<GameWorld*>(1);
        instance.Service(wrong); Check(kills == 0);
        auto disposing = frame; disposing.disposing.value = true;
        instance.Service(disposing); Check(kills == 0);
        instance.Service(frame); Check(kills == 1 && instance.Status().closed && roots.empty());
        Check(!current_owner->Owns(E()));
        Check(!current_owner->OwnedToken(E()).entity);
        instance.Service(frame); Check(kills == 1);
    }
    {
        // A rebuild is a new epoch; an old generation cannot dispose it.
        NpcCreationSession instance(owner, factory, methods, references); instance.Create(input);
        const auto token = instance.Status().entities[0].token;
        Check(token.generation > 1);
        TrackedEntities().Disposed({E(), token.generation - 1});
        instance.Service(frame); Check(!instance.Status().closed);
        const auto before = reads;
        Dispose(E(), &metadata);
        auto other_world = frame; other_world.world = reinterpret_cast<GameWorld*>(1);
        instance.Service(other_world);
        Check(instance.Status().closed && roots.empty() && reads == before && kills == 1);
    }
    {
        factory_mode = 1;
        NpcCreationSession failed(owner, factory, methods, references); failed.Create(input);
        Check(!failed.Status().create_returned && failed.Status().entities.size() == 1 && roots.size() == 5);
        failed.Service(frame); Check(failed.Status().closed && roots.empty() && kills == 2);
    }
    {
        // Dispose inside the factory must not become a newly owned live entity.
        factory_mode = 2;
        std::vector<EntityOrigin> origins;
        NpcCreationSession failed(owner, factory, methods, references,[&](EntityOrigin value){origins.push_back(value);}); failed.Create(input);
        Check(failed.Status().closed && failed.Status().entities.empty() && roots.empty());
        Check(origins.size()==1 && origins[0].token.generation==factory_token.generation);
        Check(failed.Status().creation.disposed_entities.size() == 1 && !failed.Status().creation.Unique());
        failed.Service(frame); Check(kills == 2);
    }
    {
        factory_mode = 3;
        NpcCreationSession multiple(owner, factory, methods, references); multiple.Create(input);
        Check(multiple.Status().entities.size() == 2);
        multiple.Service(frame); Check(multiple.Status().closed && roots.empty() && kills == 4);
    }
    {
        factory_mode = 0; kill_fault = true;
        NpcCreationSession uncertain(owner, factory, methods, references); uncertain.Create(input); uncertain.Remove();
        uncertain.Service(frame); uncertain.Service(frame);
        Check(kills == 5 && !uncertain.Status().closed && roots.size() == 5);
        Check(uncertain.Status().entities[0].retirement.request.exception == 0xE0510003);
        kill_fault = false; Dispose(E(), &metadata); uncertain.Service(frame);
        Check(uncertain.Status().closed && roots.empty());
    }
    {
        factory_mode = 3; cascade = true;
        NpcCreationSession children(owner, factory, methods, references); children.Create(input);
        children.Service(frame);
        Check(children.Status().closed && roots.empty() && kills == 6);
        cascade = false; factory_mode = 0;
    }
    Check(creations == 7 && setter_calls == 6);
    {
        // Copies represent one client, not new clients; rebinding an external
        // handle cannot change the identity retained by an existing session.
        EntityOwner retained = owner;
        EntityOwner assigned;
        assigned = retained;
        NpcCreationSession first(assigned, factory, methods, references);
        assigned = unrelated;
        first.Create(input);
        Check(retained.Owns(E()) && owner.Owns(E()) && !assigned.Owns(E()));
        const auto original = retained.OwnedToken(E());
        // A second client's factory returning an existing entity gives no
        // permission to adopt it, retire it, or rewrite the first identity.
        NpcCreationSession second(unrelated, M(ReturnExisting), methods, references);
        second.Create(input);
        Check(second.Status().closed && !second.Status().create_returned);
        Check(second.Status().entities.empty() && !second.Status().errors.empty());
        Check(roots.size() == 5 && !unrelated.Owns(E()));
        Check(retained.OwnedToken(E()).generation == original.generation);
        second.Remove(); second.Service(frame);
        Check(kills == 6);
        first.Remove(); first.Service(frame);
        Check(first.Status().closed && roots.empty() && kills == 7);
        Check(!retained.Owns(E()) && !owner.Owns(E()));
    }
    {
        // The same managed address can later belong to a different client,
        // but old generations and ownership checks must not follow it.
        current_owner = &unrelated; unrelated_owner = &owner;
        NpcCreationSession next(unrelated, factory, methods, references);
        next.Create(input);
        Check(unrelated.Owns(E()) && !owner.Owns(E()));
        next.Remove(); next.Service(frame);
        Check(next.Status().closed && roots.empty() && kills == 8);
    }
    {
        struct Objects:ManagedObjects {
            ObjectResult ManagedType(const char*,const char*) override {throw std::runtime_error("unexpected type lookup");}
            ObjectResult NewStringUtf8(const char*) override {throw std::runtime_error("unexpected string allocation");}
            ObjectResult AllocateObject(const char*,const char*) override {throw std::runtime_error("unexpected managed allocation");}
        } provider;
        EntityOwner sdk_owner;
        const auto create=[&]{
            PresentationBindings bindings;bindings.factory=factory;bindings.entity=methods;bindings.references=references;
            PresentationContext context(provider,bindings);sdk_owner=context.Owner();return context.Npcs();
        }();
        current_owner=&sdk_owner;unrelated_owner=&owner;
        auto npc=create();Check(roots.empty()); // construction has no game side effects
        npc->Create(input);Check(npc->Status().create_returned && sdk_owner.Owns(E()) && !owner.Owns(E()));
        npc->Remove();npc->Service(frame);Check(npc->Status().closed && roots.empty() && !sdk_owner.Owns(E()));
    }
    RemoveEntityLifecycleHooksAtQuiescentPoint();
    std::cout << "NPC creation/loading/removal/natural-exit/rebuild tests passed through SDK and local lifecycle hooks; no game acceptance.\n";
}
