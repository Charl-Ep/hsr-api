#pragma once
#include <hksr/model/ModelProfile.hpp>
#include <hksr/managed/ManagedObjects.hpp>
namespace hksr::api::detail {
ObjectResult ResolveCharacterModelType(ManagedObjects&);
ObjectResult ResolveModelPrefabType(ManagedObjects&);
ObjectResult ResolveModelAnimatorType(ManagedObjects&);
ModelMethods ResolveModelChangeBindings(MethodResolver&);
struct ModelReleaseBindings {
    Method art_unload, root_release;
};
ModelReleaseBindings ResolveModelReleaseBindings(MethodResolver&);
Method ResolveModelWeaponVisibility(MethodResolver&);
}
