#include "LifecycleBindings.hpp"
#include "../../src/metadata/ResolveBatch.hpp"

namespace hksr::api::detail {
std::array<Method, 3> ResolveEntityLifecycleEntries(MethodResolver& resolver) {
    ResolveBatch batch(resolver);
    const auto create = batch.Resolve("RPG.GameCore", "GameWorld", "CreateEmptyEntity", "RPG.GameCore.GameEntity",
                                      {"RPG.GameCore.TeamType", "RPG.GameCore.EntityType"});
    const auto dispose = batch.Resolve("RPG.GameCore", "GameEntity", "Dispose", "System.Void", {});
    const auto create_new =
        batch.Resolve("RPG.GameCore", "GameWorld", "_CreateNewEntityImpl", "RPG.GameCore.GameEntity",
                      {"System.UInt32", "RPG.GameCore.TeamType", "RPG.GameCore.EntityType"});
    // Creation attribution needs the matching disposal boundary before it can
    // expose owned lifetime tokens to clients.
    batch.RequireComplete("entity lifecycle");
    return {create, dispose, create_new};
}
}
