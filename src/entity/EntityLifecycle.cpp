#include <hksr/entity/EntityLifecycle.hpp>
#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <memory>
#include <stdexcept>
#include <thread>
#include <Windows.h>

namespace hksr::sdk_v1 {
namespace {
// Lifetime identity, not an engine RuntimeID. Shared between birth receipts
// and tracked objects so adoption doesn't create a second identity.
std::atomic<std::uint64_t> birth_sequence{0};
std::uint64_t NextBirth(){return birth_sequence.fetch_add(1,std::memory_order_relaxed)+1;}
}
struct EntityCreationScope::State {
    static thread_local State* current;
    State* previous;
    GameWorld* world;
    const void* owner;
    CreationReceipt receipt;
    std::map<GameEntity*,std::uint64_t> births;
    std::map<GameEntity*,std::uint64_t> retiring;
    BirthObserver observe,before_dispose;
    bool NotifyCpp(EntityOrigin origin) noexcept {
        try {before_dispose(origin);return true;}catch(...){return false;}
    }
    bool Notify(EntityOrigin origin) noexcept {
        __try{return NotifyCpp(origin);}
        __except(EXCEPTION_EXECUTE_HANDLER){return false;}
    }
};
thread_local EntityCreationScope::State* EntityCreationScope::State::current = nullptr;
EntityCreationScope::EntityCreationScope(GameWorld* world,const void* owner,BirthObserver observe)
    :EntityCreationScope(world,owner,std::move(observe),{}){}
EntityCreationScope::EntityCreationScope(GameWorld* world,const void* owner,BirthObserver observe,BirthObserver before_dispose)
    : state_(new State{State::current, world, owner, {}, {},{},std::move(observe),std::move(before_dispose)}) {
    State::current = state_;
}
EntityCreationScope::~EntityCreationScope() {
    State::current = state_->previous;
    delete state_;
}
CreationReceipt EntityCreationScope::Receipt() const { return state_->receipt; }
bool EntityCreationScope::ContainsLive(GameEntity* entity) noexcept {
    if (!entity) return false;
    for (auto* state = State::current; state; state = state->previous) {
        const auto& receipt = state->receipt;
        if (std::find(receipt.entities.begin(), receipt.entities.end(), entity) != receipt.entities.end())
            return std::find(receipt.disposed_entities.begin(), receipt.disposed_entities.end(), entity) == receipt.disposed_entities.end();
    }
    return false;
}
bool EntityCreationScope::ContainsOwned(GameEntity* entity,const void* owner) noexcept {
    return OwnedToken(entity,owner).entity!=nullptr;
}
EntityToken EntityCreationScope::OwnedToken(GameEntity* entity,const void* owner) noexcept {
    if(!entity || !owner)return {};
    for(auto* state=State::current;state;state=state->previous) {
        const auto& receipt=state->receipt;
        if(std::find(receipt.entities.begin(),receipt.entities.end(),entity)==receipt.entities.end())continue;
        if(state->owner!=owner || std::find(receipt.disposed_entities.begin(),receipt.disposed_entities.end(),entity)!=receipt.disposed_entities.end())return {};
        const auto found=state->births.find(entity);
        return found==state->births.end()?EntityToken{}:EntityToken{entity,found->second};
    }
    return {};
}
void EntityCreationScope::FactoryCompleted(GameWorld* world, GameEntity* entity) noexcept {
    auto* state = State::current;
    if (!state || state->world != world || !entity) return;
    try {
        auto& entities = state->receipt.entities;
        auto& disposed = state->receipt.disposed_entities;
        const bool new_birth=state->births.find(entity)==state->births.end() ||
            std::find(disposed.begin(),disposed.end(),entity)!=disposed.end();
        if(new_birth)
            state->births.insert_or_assign(entity,NextBirth());
        if (std::find(entities.begin(), entities.end(), entity) == entities.end()) entities.push_back(entity);
        disposed.erase(std::remove(disposed.begin(), disposed.end(), entity), disposed.end());
        // Receipt exists before the observer; its failure doesn't erase the
        // object that cleanup still owns. No callback on a repeated boundary.
        if(new_birth && state->observe)state->observe({{entity,state->births.at(entity)},world});
    } catch (...) { state->receipt.incomplete = true; }
}
void EntityCreationScope::DisposalCompleted(GameEntity* entity) noexcept {
    // Nested work may dispose an object created by the outer scope. Only a
    // scope that actually recorded creation can consume this notification.
    for (auto* state = State::current; state; state = state->previous) {
        const auto& entities = state->receipt.entities;
        if (std::find(entities.begin(), entities.end(), entity) == entities.end()) continue;
        try {
            auto& disposed = state->receipt.disposed_entities;
            if (std::find(disposed.begin(), disposed.end(), entity) == disposed.end()) disposed.push_back(entity);
        } catch (...) { state->receipt.incomplete = true; }
    }
}
EntityDisposalNotice EntityCreationScope::DisposalStarting(GameEntity* entity) noexcept {
    EntityDisposalNotice result;
    for(auto* state=State::current;state;state=state->previous) {
        const auto birth=state->births.find(entity);
        if(birth==state->births.end())continue;
        const auto& disposed=state->receipt.disposed_entities;
        if(std::find(disposed.begin(),disposed.end(),entity)!=disposed.end())return result;
        if(!state->before_dispose)return result; // never borrow another scope's handler
        try {
            const auto prior=state->retiring.find(entity);
            if(prior!=state->retiring.end() && prior->second==birth->second)return result;
            state->retiring.insert_or_assign(entity,birth->second);
            ++result.invoked;
            if(!state->Notify({{entity,birth->second},state->world}))++result.failed;
        }catch(...){++result.failed;}
        return result;
    }
    return result;
}

struct EntityLifecycle::State {
    struct Observer {
        EntityToken token;std::thread::id thread;std::function<void(EntityToken)> callback;
        std::atomic<bool> active{true};
        Observer(EntityToken t,std::function<void(EntityToken)> cb)
            :token(t),thread(std::this_thread::get_id()),callback(std::move(cb)){}
    };
    struct Entry { std::uint64_t generation; bool disposed; const void* owner; bool retiring=false; };
    mutable std::mutex mutex;
    std::map<GameEntity*, Entry> entries;
    std::map<EntityDisposalSubscription,std::shared_ptr<Observer>> observers;
    EntityDisposalSubscription next=0;
    static bool InvokeCpp(Observer* observer) noexcept {
        try{observer->callback(observer->token);return true;}catch(...){return false;}
    }
    static bool Invoke(Observer* observer) noexcept {
        __try{return InvokeCpp(observer);}
        __except(EXCEPTION_EXECUTE_HANDLER){return false;}
    }
};
EntityLifecycle::EntityLifecycle() : state_(new State) {}
EntityLifecycle::~EntityLifecycle() { delete state_; }
EntityToken EntityLifecycle::Track(GameEntity* entity,const void* owner) {
    if (!entity) throw std::invalid_argument("cannot own a null entity");
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (state_->entries.count(entity)) throw std::logic_error("entity already has an owner in this registry");
    const auto birth=EntityCreationScope::OwnedToken(entity,owner);
    const auto generation = birth.entity?birth.generation:NextBirth();
    state_->entries.emplace(entity, State::Entry{generation, false,owner});
    return {entity, generation};
}
bool EntityLifecycle::IsOwnedBy(GameEntity* entity,const void* owner) const {
    return OwnedToken(entity,owner).entity!=nullptr;
}
EntityToken EntityLifecycle::OwnedToken(GameEntity* entity,const void* owner) const {
    if(!entity || !owner)return {};
    std::lock_guard<std::mutex> lock(state_->mutex);
    const auto found=state_->entries.find(entity);
    return found!=state_->entries.end() && !found->second.disposed && found->second.owner==owner?
        EntityToken{entity,found->second.generation}:EntityToken{};
}
EntityToken EntityLifecycle::Current(GameEntity* entity) const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    const auto found = state_->entries.find(entity);
    return found == state_->entries.end() ? EntityToken{} : EntityToken{entity, found->second.generation};
}
EntityDisposalSubscription EntityLifecycle::BeforeDispose(EntityToken token,std::function<void(EntityToken)> callback) {
    if(!callback)throw std::invalid_argument("entity disposal callback absent");
    auto observer=std::make_shared<State::Observer>(token,std::move(callback));
    std::lock_guard<std::mutex> lock(state_->mutex);
    const auto found=state_->entries.find(token.entity);
    if(found==state_->entries.end() || found->second.generation!=token.generation || found->second.disposed || found->second.retiring)
        throw std::logic_error("entity disposal observation requires a live tracked generation");
    const auto id=++state_->next;state_->observers.emplace(id,std::move(observer));return id;
}
void EntityLifecycle::CancelBeforeDispose(EntityDisposalSubscription id) {
    std::shared_ptr<State::Observer> removed;
    std::lock_guard<std::mutex> lock(state_->mutex);
    const auto found=state_->observers.find(id);
    if(found==state_->observers.end())return;
    found->second->active=false;removed=std::move(found->second);state_->observers.erase(found);
}
EntityDisposalNotice EntityLifecycle::DisposalStarting(EntityToken token) {
    EntityDisposalNotice result;
    std::vector<std::pair<EntityDisposalSubscription,std::shared_ptr<State::Observer>>> callbacks;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        const auto found=state_->entries.find(token.entity);
        if(found==state_->entries.end() || found->second.generation!=token.generation || found->second.disposed || found->second.retiring)return result;
        for(const auto& [id,observer]:state_->observers)
            if(observer->token.entity==token.entity && observer->token.generation==token.generation)callbacks.emplace_back(id,observer);
        found->second.retiring=true;
    }
    // No registry lock across callbacks. A callback can cancel another
    // subscription or retire a different target without deadlocking.
    for(const auto& [id,observer]:callbacks) {
        if(observer->active.exchange(false)) {
            if(observer->thread!=std::this_thread::get_id())++result.wrong_thread;
            else {++result.invoked;if(!State::Invoke(observer.get()))++result.failed;}
        }
        CancelBeforeDispose(id);
    }
    return result;
}
bool EntityLifecycle::IsRetiring(EntityToken token) const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    const auto found=state_->entries.find(token.entity);
    return found!=state_->entries.end() && found->second.generation==token.generation && found->second.retiring;
}
void EntityLifecycle::Disposed(EntityToken token) {
    std::lock_guard<std::mutex> lock(state_->mutex);
    const auto found = state_->entries.find(token.entity);
    if (found != state_->entries.end() && found->second.generation == token.generation) found->second.disposed = true;
}
bool EntityLifecycle::IsDisposed(EntityToken token) const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    const auto found = state_->entries.find(token.entity);
    return found != state_->entries.end() && found->second.generation == token.generation && found->second.disposed;
}
void EntityLifecycle::Forget(EntityToken token) {
    std::vector<std::shared_ptr<State::Observer>> removed;
    std::lock_guard<std::mutex> lock(state_->mutex);
    const auto found = state_->entries.find(token.entity);
    if (found != state_->entries.end() && found->second.generation == token.generation) {
        state_->entries.erase(found);
        for(auto it=state_->observers.begin();it!=state_->observers.end();) {
            if(it->second->token.entity==token.entity && it->second->token.generation==token.generation) {
                it->second->active=false;removed.push_back(std::move(it->second));it=state_->observers.erase(it);
            }else ++it;
        }
    }
}
}
