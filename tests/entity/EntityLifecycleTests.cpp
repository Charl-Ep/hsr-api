#include <hksr/entity/EntityLifecycle.hpp>
#include <Windows.h>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace hksr::api;
void Check(bool ok) { if (!ok) throw std::runtime_error("entity lifecycle test failed"); }
int world, foreign_world, first, second;
auto* W = reinterpret_cast<GameWorld*>(&world);
auto* A = reinterpret_cast<GameEntity*>(&first);
auto* B = reinterpret_cast<GameEntity*>(&second);
GameEntity* __fastcall PartialFactory(EntityManager*, MapNpcDefinition*, NpcDataRow*, EntityCallback*, bool, void*) {
    EntityCreationScope::FactoryCompleted(W, A);
    RaiseException(0xE0424343, 0, 0, nullptr);
    return nullptr;
}
}
void TestEntityLifecycle() {
    int owner,other_owner;
    EntityLifecycle adopted;
    EntityToken born;
    {
        EntityCreationScope owned(W,&owner);
        EntityCreationScope::FactoryCompleted(W,A);
        born=EntityCreationScope::OwnedToken(A,&owner);
        Check(born.entity==A && born.generation!=0);
        Check(!EntityCreationScope::OwnedToken(A,&other_owner).entity);
        EntityCreationScope::FactoryCompleted(W,A);
        Check(EntityCreationScope::OwnedToken(A,&owner).generation==born.generation);
        const auto tracked=adopted.Track(A,&owner);
        Check(tracked.generation==born.generation && adopted.OwnedToken(A,&owner).generation==born.generation);
        Check(EntityCreationScope::ContainsOwned(A,&owner) && !EntityCreationScope::ContainsOwned(A,&other_owner));
        {
            EntityCreationScope unrelated(W,&other_owner);
            EntityCreationScope::FactoryCompleted(W,B);
            Check(!EntityCreationScope::ContainsOwned(B,&owner) && EntityCreationScope::ContainsOwned(A,&owner));
        }
        EntityCreationScope::DisposalCompleted(A);
        Check(!EntityCreationScope::ContainsOwned(A,&owner));
        Check(!EntityCreationScope::OwnedToken(A,&owner).entity);
    }
    Check(!EntityCreationScope::ContainsOwned(A,&owner));
    // The real hook publishes disposal independently to both receipt/registry.
    Check(adopted.OwnedToken(A,&owner).generation==born.generation);
    adopted.Disposed(born);Check(!adopted.OwnedToken(A,&owner).entity);adopted.Forget(born);
    {
        EntityCreationScope rebirth(W,&owner);
        EntityCreationScope::FactoryCompleted(W,A);
        const auto first_birth=EntityCreationScope::OwnedToken(A,&owner);
        Check(first_birth.generation!=born.generation);
        EntityCreationScope::DisposalCompleted(A);
        EntityCreationScope::FactoryCompleted(W,A);
        const auto second_birth=EntityCreationScope::OwnedToken(A,&owner);
        Check(first_birth.generation!=second_birth.generation);
        const auto tracked=adopted.Track(A,&owner);
        Check(tracked.generation==second_birth.generation);
        adopted.Disposed(first_birth);adopted.Forget(first_birth);
        Check(adopted.OwnedToken(A,&owner).generation==second_birth.generation);
        adopted.Disposed(tracked);adopted.Forget(tracked);
    }
    EntityCreationScope::FactoryCompleted(W, B); // no transaction: never adopted
    Check(!EntityCreationScope::ContainsLive(B) && !EntityCreationScope::ContainsLive(nullptr));
    {
        EntityCreationScope outer(W);
        EntityCreationScope::FactoryCompleted(reinterpret_cast<GameWorld*>(&foreign_world), B);
        std::thread foreign([] { EntityCreationScope::FactoryCompleted(W, B); }); foreign.join();
        Check(outer.Receipt().entities.empty());
        Check(!EntityCreationScope::ContainsLive(B));
        {
            EntityCreationScope nested(W);
            EntityCreationScope::FactoryCompleted(W, B);
            Check(nested.Receipt().Unique() == B);
            Check(EntityCreationScope::ContainsLive(B));
        }
        Check(outer.Receipt().entities.empty());
        Check(!EntityCreationScope::ContainsLive(B)); // nested receipt does not leak outward
        EntityFactory factory({reinterpret_cast<void*>(&PartialFactory), nullptr});
        const auto result = factory.CreateMapNpc(nullptr, nullptr, nullptr, nullptr, false);
        Check(!result.call.returned && result.call.exception == 0xE0424343 && !result.entity);
        Check(outer.Receipt().Unique() == A);
        Check(EntityCreationScope::ContainsLive(A));
        bool seen_on_foreign_thread=true;
        std::thread receipt_reader([&] { seen_on_foreign_thread=EntityCreationScope::ContainsLive(A); });
        receipt_reader.join();Check(!seen_on_foreign_thread);
        EntityCreationScope::FactoryCompleted(W, A); // repeated boundary, not a second actor
        Check(outer.Receipt().entities.size() == 1);
        EntityCreationScope::FactoryCompleted(W, B);
        Check(!outer.Receipt().Unique() && outer.Receipt().entities.size() == 2);
        {
            EntityCreationScope nested(W);
            Check(EntityCreationScope::ContainsLive(A)); // known outer creation, not whole-world membership
            EntityCreationScope::DisposalCompleted(A);
            Check(!EntityCreationScope::ContainsLive(A));
            Check(nested.Receipt().disposed_entities.empty());
        }
        Check(outer.Receipt().Unique() == B);
        EntityCreationScope::FactoryCompleted(W, A); // address reused inside the same outer call
        Check(outer.Receipt().disposed_entities.empty() && !outer.Receipt().Unique());
        Check(EntityCreationScope::ContainsLive(A)); // newly recorded lifetime at the reused address
    }
    EntityCreationScope clean(W); Check(clean.Receipt().entities.empty());
    Check(!EntityCreationScope::ContainsLive(A) && !EntityCreationScope::ContainsLive(B));
    EntityLifecycle lifecycle;
    const auto first_token = lifecycle.Track(A,&owner);
    Check(lifecycle.IsOwnedBy(A,&owner) && !lifecycle.IsOwnedBy(A,&other_owner) && !lifecycle.IsOwnedBy(A,nullptr));
    bool duplicate = false;
    try { lifecycle.Track(A); } catch (const std::logic_error&) { duplicate = true; }
    Check(duplicate && !lifecycle.IsDisposed(first_token));
    lifecycle.Disposed(first_token); Check(lifecycle.IsDisposed(first_token));
    Check(!lifecycle.IsOwnedBy(A,&owner));
    lifecycle.Forget(first_token);
    const auto reused = lifecycle.Track(A);
    Check(!lifecycle.IsOwnedBy(A,&owner));
    Check(reused.generation != first_token.generation);
    lifecycle.Disposed(first_token); lifecycle.Forget(first_token);
    Check(!lifecycle.IsDisposed(reused) && lifecycle.Current(A).generation == reused.generation);
    lifecycle.Disposed(reused); Check(lifecycle.IsDisposed(reused));
    std::cout << "Entity receipts/ownership tests passed.\n";
}
