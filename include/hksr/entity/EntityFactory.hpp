#pragma once
#include "../metadata/MethodResolver.hpp"

namespace hksr::sdk_v1 {
// Opaque managed objects, not structs reconstructed by application code.
struct EntityManager;
struct MapNpcDefinition;
struct NpcDataRow;
struct GameEntity;
struct EntityCallback;
struct CreateEntityResult { CallResult call; GameEntity* entity=nullptr; };

// Thin official boundary. It neither fabricates NPC definitions nor claims
// that model loading/effects/cleanup completed when the call returns.
class HKSR_API EntityFactory {
public:
    explicit EntityFactory(Method method):create_map_(method){}
    CreateEntityResult CreateMapNpc(EntityManager* manager,MapNpcDefinition* definition,
        NpcDataRow* row,EntityCallback* callback,bool option) const;
private:
    Method create_map_;
};
HKSR_API Method ResolveMapNpcFactory(MethodResolver& resolver);
}
