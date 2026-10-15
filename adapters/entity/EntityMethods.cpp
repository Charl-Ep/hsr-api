#include <hksr/entity/EntityFactory.hpp>

namespace hksr::sdk_v1 {
Method ResolveMapNpcFactory(MethodResolver& resolver){
    return resolver.Resolve("RPG.GameCore","EntityManagerExtension","ClientCreateMapNPC","RPG.GameCore.GameEntity",
        {"RPG.GameCore.EntityManager","RPG.Client.MapNpcDef","RPG.GameCore.NPCDataRow",
         "System.Action<RPG.GameCore.GameEntity>","System.Boolean"},MethodKind::Static);
}
}
