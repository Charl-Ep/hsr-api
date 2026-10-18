set(HKSR_API_SOURCES
    src/runtime/OperationTrace.cpp

    # adapters/animation
    adapters/animation/AnimatorLayout.cpp
    adapters/animation/AdventureConfigMethods.cpp
    adapters/animation/AnimationComponentTypes.cpp
    adapters/animation/AnimationControlMethods.cpp
    adapters/animation/AnimationEventMethods.cpp

    # adapters/control
    adapters/control/ActorControlMethods.cpp
    adapters/control/PartnerControlMethods.cpp

    # adapters/entity
    adapters/entity/EntityAccessMethods.cpp
    adapters/entity/EntityMethods.cpp
    adapters/entity/PartnerEventMethods.cpp

    # adapters/managed
    adapters/managed/ArrayMethods.cpp
    adapters/managed/ReferenceMethods.cpp

    # adapters/model
    adapters/model/ArtAdmissionMethods.cpp
    adapters/model/ModelMethods.cpp
    adapters/model/ProfileInputMethods.cpp

    # adapters/presentation
    adapters/presentation/TimelineBoundaryMethods.cpp
    adapters/presentation/EffectAccessMethods.cpp
    adapters/presentation/EffectControlMethods.cpp
    adapters/presentation/EffectEntityFollowMethods.cpp
    adapters/presentation/EffectActionMethods.cpp
    adapters/presentation/EffectParameterMethods.cpp
    adapters/presentation/EffectCompletionMethods.cpp

    # adapters/runtime
    adapters/runtime/MetadataBootstrap.cpp

    # adapters/unity
    adapters/unity/ComponentConfigMethods.cpp
    adapters/unity/MeshSkinningMethods.cpp
    adapters/unity/RendererMethods.cpp
    adapters/unity/SceneObjectMethods.cpp
    adapters/unity/UnityAssetMethods.cpp
    adapters/unity/UnityLifetimeMethods.cpp

    # adapters/world
    adapters/world/WorldMapMethods.cpp

    # src/animation
    src/animation/ActorAnimationSession.cpp
    src/animation/ActionRelations.cpp
    src/animation/AdventureConfig.cpp
    src/animation/AnimationControl.cpp
    src/animation/AnimationOverrides.cpp
    src/animation/AnimationResources.cpp
    src/animation/AnimatorCatalog.cpp
    src/animation/ControllerBinding.cpp
    src/animation/OfficialCatalog.cpp
    src/animation/catalog/VisualCatalog.cpp
    src/animation/catalog/PhaseVisualCatalog.cpp
    src/animation/catalog/CharacterCatalog.cpp
    src/animation/catalog/ActionConfigCatalog.cpp
    src/animation/catalog/ReadyCatalog.cpp
    src/animation/catalog/AbilityCatalog.cpp
    src/animation/catalog/NpcCatalog.cpp

    # src/control
    src/control/ActorControl.cpp
    src/control/ActorControlSession.cpp
    src/control/PartnerControlSession.cpp
    src/control/LookSupport.cpp
    src/control/MouseMode.cpp

    # src/entity
    src/entity/EntityAccess.cpp
    src/entity/EntityFactory.cpp
    src/entity/EntityLifecycle.cpp
    src/entity/EntityOwner.cpp
    src/entity/PartnerEventCleanup.cpp
    src/entity/EntityProvenance.cpp
    src/entity/LifecycleHooks.cpp
    src/entity/MapNpcInputs.cpp
    src/entity/NpcCreationSession.cpp
    src/entity/EntityCreationSession.cpp
    src/animation/ActorEmotion.cpp
    src/model/ActorInputPolicy.cpp
    src/model/ModelVisibilityLease.cpp
    src/entity/NpcInputConstruction.cpp

    # src/managed
    src/managed/ManagedArrays.cpp
    src/managed/ManagedLists.cpp
    src/managed/ManagedRoots.cpp
    src/managed/References.cpp

    # src/metadata
    src/metadata/MetadataResolver.cpp
    src/metadata/RuntimeMetadata.cpp

    # src/model
    src/model/ArtAdmission.cpp
    src/model/CharacterOutfit.cpp
    src/model/ModelBaselineCapture.cpp
    src/model/ModelProfile.cpp
    src/model/ModelProfileInputs.cpp
    src/model/ModelProfileRequest.cpp
    src/model/ModelRelease.cpp
    src/model/ModelTransition.cpp
    src/model/ModelServices.cpp
    src/model/ProfileInputs.cpp

    # src/platform
    src/platform/NativeHooks.cpp

    # src/presentation
    src/presentation/PresentationBindings.cpp
    src/presentation/PresentationContext.cpp
    src/presentation/PresentationResources.cpp
    src/presentation/PresentationIsolation.cpp
    src/presentation/PresentationSources.cpp

    # src/presentation/effects
    src/presentation/effects/EffectAccess.cpp
    src/presentation/effects/EffectControl.cpp
    src/presentation/effects/EffectEntityFollow.cpp
    src/presentation/effects/EffectActions.cpp
    src/presentation/effects/EffectAnimatorParameters.cpp
    src/presentation/effects/EffectCompletion.cpp
    src/presentation/effects/EffectInputs.cpp
    src/presentation/effects/EffectQueries.cpp
    src/presentation/effects/EffectRelations.cpp
    src/presentation/effects/EffectTriggerSource.cpp
    src/presentation/effects/EffectScopes.cpp
    src/presentation/effects/EffectSubmissions.cpp
    src/presentation/effects/OwnedEffectRequest.cpp
    src/presentation/effects/PresentationEffects.cpp

    # src/presentation/events
    src/presentation/events/AnimationEventPolicy.cpp
    src/presentation/events/AnimationConstructionRoute.cpp
    src/presentation/events/AnimationEventConstruction.cpp
    src/presentation/events/AnimationEventDefinitions.cpp
    src/presentation/events/AnimationSequenceExecutions.cpp
    src/presentation/events/AnimationEvents.cpp
    src/presentation/events/AnimationExitConfig.cpp
    src/presentation/events/EventRegistrationLease.cpp
    src/presentation/events/NpcEventSupplement.cpp
    src/presentation/events/EventValueInputs.cpp
    src/presentation/events/ManagedConfigProjection.cpp

    # src/presentation/timeline
    src/presentation/timeline/EventRecordLifetime.cpp
    src/presentation/timeline/TimelineAsset.cpp
    src/presentation/timeline/OwnedTimeline.cpp
    src/presentation/timeline/CommonTimelinePlayback.cpp
    src/presentation/timeline/TimelineCallScope.cpp
    src/presentation/timeline/TimelineEnvironment.cpp
    src/presentation/timeline/TimelineGraphAccess.cpp
    src/presentation/timeline/TimelineRegistryAccess.cpp
    src/presentation/timeline/TimelineRegistryHooks.cpp
    src/presentation/timeline/TimelineRegistryTracking.cpp
    src/presentation/timeline/TimelineTasks.cpp
    src/presentation/timeline/TimelineWaits.cpp

    # src/runtime
    src/runtime/RuntimeHost.cpp
    src/runtime/RuntimeThread.cpp

    # src/serialization
    src/serialization/ComponentReferences.cpp

    # src/unity
    src/unity/AssetLeases.cpp
    src/unity/ComponentConfig.cpp
    src/unity/MeshSkinning.cpp
    src/unity/MeshWriterLocator.cpp
    src/unity/OwnedUnityObjects.cpp
    src/unity/RendererAccess.cpp
    src/unity/SceneObjects.cpp
    src/unity/UnityAssetAccess.cpp

    # src/world
    src/world/WorldFrames.cpp
    src/world/WorldMap.cpp
    src/world/WorldPlacement.cpp
)

set(HKSR_API_TEST_SOURCES

    # src/entity
    src/entity/NpcInputConstruction.cpp

    # src/presentation/events
    src/presentation/events/ManagedConfigProjection.cpp

    # src/presentation/timeline
    src/presentation/timeline/CommonTimelinePlayback.cpp
    src/presentation/timeline/TimelineRegistryAccess.cpp
    src/presentation/timeline/TimelineRegistryTracking.cpp
    src/presentation/timeline/TimelineWaits.cpp

    # src/unity
    src/unity/MeshWriterLocator.cpp

    # tests/animation
    tests/animation/ActionConfigTests.cpp
    tests/animation/ActionRelationsTests.cpp
    tests/animation/AnimationControlTests.cpp
    tests/animation/AnimationOverridesTests.cpp
    tests/animation/AnimationResourcesTests.cpp
    tests/animation/AnimatorCatalogTests.cpp
    tests/animation/ControllerBindingTests.cpp
    tests/animation/OfficialCatalogTests.cpp

    # tests/control
    tests/control/ActorControlSessionTests.cpp
    tests/control/LookSupportTests.cpp

    # tests/entity
    tests/entity/EntityAccessTests.cpp
    tests/entity/EntityLifecycleTests.cpp
    tests/entity/EntityProvenanceTests.cpp
    tests/entity/LifecycleHookTests.cpp
    tests/entity/NpcCreationSessionTests.cpp
    tests/entity/NpcInputTests.cpp

    # tests/managed
    tests/managed/ManagedInputTests.cpp

    # tests/metadata
    tests/metadata/MetadataLocatorTests.cpp
    tests/metadata/MetadataReaderTests.cpp

    # tests/model
    tests/model/ArtAdmissionTests.cpp
    tests/model/CharacterOutfitTests.cpp
    tests/model/ModelBaselineCaptureTests.cpp
    tests/model/ModelProfileTests.cpp
    tests/model/ModelReleaseTests.cpp
    tests/model/ModelTransitionTests.cpp

    # tests/presentation/effects
    tests/presentation/effects/EffectAccessTests.cpp
    tests/presentation/effects/EffectControlTests.cpp
    tests/presentation/effects/EffectEntityFollowTests.cpp
    tests/presentation/effects/EffectActionsTests.cpp
    tests/presentation/effects/EffectAnimatorParametersTests.cpp
    tests/presentation/effects/EffectCompletionTests.cpp
    tests/presentation/effects/EffectInputsTests.cpp
    tests/presentation/effects/EffectQueryTests.cpp
    tests/presentation/effects/EffectRelationsTests.cpp
    tests/presentation/effects/EffectTriggerSourceTests.cpp
    tests/presentation/effects/OwnedEffectRequestTests.cpp
    tests/presentation/events/NpcEventSupplementTests.cpp

    # tests/presentation/events
    tests/presentation/events/AnimationConfigSchemaTests.cpp
    tests/presentation/events/AnimationEventTests.cpp
    tests/presentation/events/AnimationExitConfigTests.cpp
    tests/presentation/events/ConfigProjectionTests.cpp
    tests/presentation/events/EventRegistrationLeaseTests.cpp
    tests/presentation/events/EventValueInputsTests.cpp
    tests/presentation/events/ManagedConfigProjectionTests.cpp

    # tests/presentation/timeline
    tests/presentation/timeline/TimelineAssetTests.cpp
    tests/presentation/timeline/OwnedTimelineTests.cpp
    tests/presentation/timeline/TimelineGraphTests.cpp
    tests/presentation/timeline/TimelineOwnershipTests.cpp
    tests/presentation/timeline/TimelineRegistryTests.cpp

    # tests/runtime
    tests/runtime/RuntimeHostTests.cpp
    tests/runtime/RuntimeThreadTests.cpp

    # tests/unity
    tests/unity/AssetLeaseTests.cpp
    tests/unity/MeshSkinningTests.cpp
    tests/unity/MeshWriterLocatorTests.cpp
    tests/unity/UnityAssetTests.cpp
    tests/unity/UnityLifetimeTests.cpp

    # tests/world
    tests/world/WorldFrameTests.cpp
    tests/world/WorldMapTests.cpp
)
