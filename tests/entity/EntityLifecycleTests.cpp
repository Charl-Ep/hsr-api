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
    {
        unsigned cleaned=0;EntityToken prior;
        EntityCreationScope outer(W,&owner,{},[&](auto origin){
            Check(origin.world==W && origin.token.entity==A && origin.token.generation!=prior.generation);
            prior=origin.token;++cleaned;
            Check(EntityCreationScope::DisposalStarting(A).invoked==0);
        });
        EntityCreationScope::FactoryCompleted(W,A);
        {
            EntityCreationScope nested(W,&other_owner,{},[](auto){throw std::runtime_error("must not borrow nested cleanup");});
            Check(EntityCreationScope::DisposalStarting(A).invoked==1 && cleaned==1);
        }
        Check(EntityCreationScope::DisposalStarting(A).invoked==0);
        EntityCreationScope::DisposalCompleted(A);EntityCreationScope::FactoryCompleted(W,A);
        Check(EntityCreationScope::DisposalStarting(A).invoked==1 && cleaned==2);
        Check(EntityCreationScope::DisposalStarting(B).invoked==0);
        EntityCreationScope nested(W,&other_owner,{},[](auto){throw std::runtime_error("cleanup error does not veto disposal");});
        EntityCreationScope::FactoryCompleted(W,B);
        const auto failed=EntityCreationScope::DisposalStarting(B);Check(failed.invoked==1 && failed.failed==1);
    }
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
    lifecycle.Forget(reused);
    const auto watched=lifecycle.Track(A),other=lifecycle.Track(B);
    int calls=0;
    EntityDisposalSubscription second_observer=0;
    lifecycle.BeforeDispose(watched,[&](auto token){
        Check(token.entity==A && token.generation==watched.generation && lifecycle.IsRetiring(token) && !lifecycle.IsDisposed(token));
        ++calls;lifecycle.CancelBeforeDispose(second_observer);
        Check(lifecycle.DisposalStarting(token).invoked==0);
        Check(lifecycle.DisposalStarting(other).invoked==1);
    });
    second_observer=lifecycle.BeforeDispose(watched,[&](auto){calls+=100;});
    lifecycle.BeforeDispose(watched,[&](auto){++calls;throw std::runtime_error("one listener failed");});
    lifecycle.BeforeDispose(watched,[&](auto){++calls;});
    lifecycle.BeforeDispose(other,[&](auto token){Check(token.entity==B);++calls;});
    Check(lifecycle.DisposalStarting(reused).invoked==0 && !lifecycle.IsRetiring(watched));
    const auto notice=lifecycle.DisposalStarting(watched);
    Check(notice.invoked==3 && notice.failed==1 && notice.wrong_thread==0 && calls==4);
    bool late=false;try{lifecycle.BeforeDispose(watched,[](auto){});}catch(const std::logic_error&){late=true;}
    Check(late && lifecycle.DisposalStarting(watched).invoked==0);
    lifecycle.Forget(watched);lifecycle.Forget(other);
    const auto foreign_thread=lifecycle.Track(A);
    lifecycle.BeforeDispose(foreign_thread,[&](auto){++calls;});
    EntityDisposalNotice wrong;
    std::thread disposing([&]{wrong=lifecycle.DisposalStarting(foreign_thread);});disposing.join();
    Check(wrong.invoked==0 && wrong.wrong_thread==1 && calls==4);
    lifecycle.Forget(foreign_thread);
    const auto forgotten=lifecycle.Track(A);
    lifecycle.BeforeDispose(forgotten,[&](auto){++calls;});lifecycle.Forget(forgotten);
    const auto replacement=lifecycle.Track(A);
    Check(lifecycle.DisposalStarting(replacement).invoked==0 && calls==4);
    lifecycle.Forget(replacement);
    std::cout << "Entity receipts/ownership tests passed.\n";
}
