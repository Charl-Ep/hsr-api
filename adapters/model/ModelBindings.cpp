#include "ModelBindings.hpp"
namespace hksr::api::detail {
ObjectResult ResolveCharacterModelType(ManagedObjects& objects) {
    return objects.ManagedType("RPG.GameCore", "CharacterModelComponent");
}
ObjectResult ResolveModelPrefabType(ManagedObjects& objects) {
    return objects.ManagedType("UnityEngine", "GameObject");
}
ObjectResult ResolveModelAnimatorType(ManagedObjects& objects) {
    return objects.ManagedType("UnityEngine", "Animator");
}
ModelMethods ResolveModelChangeBindings(MethodResolver& m) {
    ModelMethods result;
    result.can_change = m.Resolve("RPG.GameCore", "CharacterModelComponent", "CanChangeModel", "System.Boolean",
                                  {"System.String", "System.String&"});
    result.change = m.Resolve("RPG.GameCore", "CharacterModelComponent", "ChangeModel", "System.Boolean",
                              {"System.String", "UnityEngine.GameObject"});
    return result;
}
ModelReleaseBindings ResolveModelReleaseBindings(MethodResolver& m) {
    return {m.Resolve("RPG.GameCore", "CharacterModelComponent", "OnArtNodeUnload", "System.Void", {"System.Boolean"}),
            m.Resolve("RPG.GameCore", "CharacterModelComponent", "_ReleaseRootModel", "System.Void", {})};
}
Method ResolveModelWeaponVisibility(MethodResolver& m) {
    return m.Resolve("RPG.GameCore", "CharacterModelComponent", "_HideWeapons", "System.Void", {"System.String[]"});
}
}
