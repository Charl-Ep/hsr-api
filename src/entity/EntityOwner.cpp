#include <hksr/entity/EntityOwner.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <memory>

namespace hksr::sdk_v1 {
struct EntityOwner::State {
    struct Identity {};
    std::shared_ptr<Identity> identity = std::make_shared<Identity>();
};
EntityOwner::EntityOwner() : state_(new State) {}
EntityOwner::EntityOwner(const EntityOwner& other) : state_(new State(*other.state_)) {}
EntityOwner& EntityOwner::operator=(const EntityOwner& other) {
    state_->identity = other.state_->identity;
    return *this;
}
EntityOwner::~EntityOwner() { delete state_; }
const void* EntityOwner::Identity() const { return state_->identity.get(); }
bool EntityOwner::Owns(GameEntity* entity) const { return OwnedToken(entity).entity != nullptr; }
EntityToken EntityOwner::OwnedToken(GameEntity* entity) const {
    const auto creating = EntityCreationScope::OwnedToken(entity, Identity());
    return creating.entity ? creating : TrackedEntities().OwnedToken(entity, Identity());
}
}
