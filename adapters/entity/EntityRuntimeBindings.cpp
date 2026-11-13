#include "EntityRuntimeBindings.hpp"
#include <hksr/entity/EntityCreationSession.hpp>
#include <stdexcept>
namespace hksr::api {
Method ResolveStoryActorFactory(MethodResolver& m) {
    return m.Resolve("RPG.GameCore", "EntityManagerExtension", "ClientCreateStoryNPCEntity", "RPG.GameCore.GameEntity",
                     {"RPG.GameCore.EntityManager", "RPG.GameCore.NPCSubType", "System.String", "System.String",
                      "System.String", "System.String", "RPG.GameCore.StoryCharacterRow", "System.String",
                      "System.String", "System.String[]", "System.Boolean", "System.Action<RPG.GameCore.GameEntity>"},
                     MethodKind::Static);
}
Method ResolvePartnerFactory(MethodResolver& m) {
    return m.Resolve("RPG.GameCore", "EntityManagerExtension", "CreatePartner", "RPG.GameCore.GameEntity",
                     {"RPG.GameCore.EntityManager", "RPG.GameCore.GameEntity", "System.String", "System.String"},
                     MethodKind::Static);
}

namespace detail {
StoryRuntimeBindings ResolveStoryRuntimeBindings(RuntimeMetadata& metadata) {
    std::size_t offset = 0;
    for (const auto& field : metadata.ReadFields("RPG.GameCore", "GameComponentBase", false))
        if (field.name == "_OwnerRef" && field.type == "RPG.GameCore.GameEntity")
            offset = field.offset;
    if (offset < 16)
        throw std::runtime_error("entity component owner field unavailable");
    return {offset,
            metadata.Resolve("RPG.GameCore", "CharacterOutfitComponent", "InitComponentForStoryCharacter",
                             "System.Void", {"System.String"}),
            metadata.Resolve("", "PEPNFLNCDHG", "JHDMCDCFDDP", "System.Void", {})};
}
}
}
