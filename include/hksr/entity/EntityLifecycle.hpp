#pragma once
#include "EntityFactory.hpp"
#include <cstdint>
#include <functional>
#include <vector>

namespace hksr::sdk_v1 {
struct GameWorld;
struct EntityToken {
    GameEntity* entity = nullptr;
    std::uint64_t generation = 0;
};
// Provenance is not permission to invoke a retired entity/world. The world
// pointer is an identity captured at creation, not a later live getter result.
struct EntityOrigin {EntityToken token;GameWorld* world=nullptr;};
struct CreationReceipt {
    std::vector<GameEntity*> entities;
    std::vector<GameEntity*> disposed_entities;
    bool incomplete = false;
    // For cleanup, not proof that the outer factory/model load succeeded.
    GameEntity* Unique() const {
        if (incomplete) return nullptr;
        GameEntity* result = nullptr;
        for (auto* entity : entities) {
            bool disposed = false;
            for (auto* dead : disposed_entities) if (dead == entity) disposed = true;
            if (disposed) continue;
            if (result) return nullptr;
            result = entity;
        }
        return result;
    }
};

// Scope around one synchronous factory call on the caller's thread. Events
// from nested requests belong to the innermost scope; no time/name matching.
class HKSR_API EntityCreationScope {
public:
    using BirthObserver=std::function<void(EntityOrigin)>;
    explicit EntityCreationScope(GameWorld* world,const void* owner=nullptr,BirthObserver observe={});
    ~EntityCreationScope();
    EntityCreationScope(const EntityCreationScope&) = delete;
    EntityCreationScope& operator=(const EntityCreationScope&) = delete;
    CreationReceipt Receipt() const;
    // Exact identities already returned by CreateEmptyEntity in a live
    // synchronous scope on this thread, before the outer factory returns.
    // This is a creation receipt, NOT permanent ownership or load completion.
    // Never dereferences entity; disposed receipts and unrelated entities fail.
    static bool ContainsLive(GameEntity* entity) noexcept;
    static bool ContainsOwned(GameEntity* entity,const void* owner) noexcept;
    // A lifetime identity already available INSIDE the factory. Track with
    // the same owner before this scope ends adopts this exact generation.
    static EntityToken OwnedToken(GameEntity* entity,const void* owner) noexcept;
    // Adapter hook calls this only after CreateEmptyEntity returns normally.
    static void FactoryCompleted(GameWorld* world, GameEntity* entity) noexcept;
    // A factory may create, then dispose, before returning/throwing. Preserve
    // that outcome even before the Framework can register the returned object.
    static void DisposalCompleted(GameEntity* entity) noexcept;
private:
    struct State;
    State* state_;
};

// Product state, independent of capture enablement. No engine API is invoked
// here. Disposal is published after the original call completes, not at entry.
class HKSR_API EntityLifecycle {
public:
    EntityLifecycle();
    ~EntityLifecycle();
    EntityLifecycle(const EntityLifecycle&) = delete;
    EntityLifecycle& operator=(const EntityLifecycle&) = delete;
    EntityToken Track(GameEntity* entity,const void* owner=nullptr);
    bool IsOwnedBy(GameEntity* entity,const void* owner) const;
    EntityToken OwnedToken(GameEntity* entity,const void* owner) const;
    EntityToken Current(GameEntity* entity) const;
    void Disposed(EntityToken token);
    bool IsDisposed(EntityToken token) const;
    void Forget(EntityToken token);
private:
    struct State;
    State* state_;
};
}
