#pragma once
#include "../world/WorldPlacement.hpp"
#include "../control/ActorControl.hpp"
#include "../animation/AnimationControl.hpp"
#include "events/AnimationEvents.hpp"
#include "../animation/AdventureConfig.hpp"
#include "../model/ArtAdmission.hpp"
#include "../unity/ComponentConfig.hpp"
#include "../entity/EntityFactory.hpp"
#include "../managed/ManagedArrays.hpp"
#include "../managed/ManagedLists.hpp"
#include "../unity/MeshSkinning.hpp"
#include "../model/ModelProfile.hpp"
#include "../unity/OwnedUnityObjects.hpp"
#include "../model/ProfileInputs.hpp"
#include "../unity/RendererAccess.hpp"
#include "../unity/SceneObjects.hpp"
#include "../unity/UnityAssetAccess.hpp"
#include "../world/WorldMap.hpp"
#include <functional>

namespace hksr::sdk_v1 {
// Native value assembly only. Resolving this does not allocate managed objects,
// create actors or load prefabs. Unavailable groups remain explicit; errors
// are not a global permission gate for unrelated operations.
struct PresentationBindings {
    Method factory;
    ReferenceMethods references;
    EntityAccessMethods entity;
    ModelAccessMethods model;
    RendererMethods renderer;
    UnityLifetimeMethods lifetime;
    ArtAdmissionMethods admission;
    VisualObjectMethods visual;
    AdventureConfigMethods config;
    AdventureDataField adventure_data;
    AnimationEventMethods events;
    ReferenceListMethods event_list;
    ProfileInputMethods profile_inputs;
    ProfileEventFields profile_events;
    ModelMethods profile;
    AnimationControlMethods animation;
    ControllerLinkField model_loader,loader_model,loader_manager;
    SceneObjectMethods scene;
    ComponentConfigMethods components;
    ArrayMethods arrays;
    HierarchyMethods hierarchy;
    MeshMethods mesh;
    MeshTemplateMethods mesh_templates;
    MeshSkinningMethods skinning;
    TextureMethods texture;
    MaterialMethods material;
    ShaderMethods shader;
    WorldMapMethods world_map;
    WorldPlacementMethods placement;
    ActorControlMethods actor_control;
    std::vector<std::string> errors;
};
// Select from the provider that actually opened this domain, not a Framework
// version constant. This does not qualify unknown future versions.
HKSR_API PresentationBindings ResolvePresentationBindings(RuntimeMetadata& metadata);
// Shared declaration-only composition for update audits. Uses the very same
// method/field requests as production; no module lookup, hooks or game calls.
// Dynamic constructed event-list types and native skinning remain empty here
// and are completed by the RuntimeMetadata overload. This is not readiness.
using DeclaredFieldLookup=std::function<std::vector<DeclaredField>(const char*,const char*)>;
HKSR_API PresentationBindings ResolvePresentationDeclarations(MethodResolver& methods,
    const DeclaredFieldLookup& fields);
}
