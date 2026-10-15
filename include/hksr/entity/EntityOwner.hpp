#pragma once
#include "EntityLifecycle.hpp"

namespace hksr::sdk_v1 {
// A client ownership scope. Copies share the same scope; default construction
// creates a distinct scope. Sessions retain a copy, so a temporary handle may
// be released without invalidating a running creation. No destructor calls the
// game or cancels sessions. Engine-thread queries include the synchronous
// factory window, before the returned entity has been adopted into the registry.
class HKSR_API EntityOwner {
public:
    EntityOwner();
    EntityOwner(const EntityOwner& other);
    EntityOwner& operator=(const EntityOwner& other);
    ~EntityOwner();
    bool Owns(GameEntity* entity) const;
    EntityToken OwnedToken(GameEntity* entity) const;
private:
    friend class NpcCreationSession;
    const void* Identity() const;
    struct State;
    State* state_;
};
}
