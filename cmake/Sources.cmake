set(HKSR_API_SOURCES
    src/runtime/OperationTrace.cpp
    src/presentation/attachments/AttachmentVisibility.cpp
    src/presentation/attachments/AttachmentVisibilityNative.cpp
    src/presentation/attachments/AttachmentScale.cpp
    src/presentation/attachments/AttachmentScaleNative.cpp
    src/presentation/materials/FaceMaterialComponents.cpp

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
    adapters/managed/GenericFactoryMethods.cpp
    adapters/managed/ReferenceMethods.cpp

    # adapters/model
    adapters/model/ModelAccessMethods.cpp
    adapters/model/ArtAdmissionMethods.cpp
    adapters/model/ModelMethods.cpp
    adapters/model/ProfileInputMethods.cpp
    adapters/model/VisualConfigurationBindings.cpp

    # adapters/presentation
    adapters/presentation/TimelineBoundaryMethods.cpp
    adapters/presentation/EffectAccessMethods.cpp
    adapters/presentation/EffectControlMethods.cpp
    adapters/presentation/EffectEntityFollowMethods.cpp
    adapters/presentation/EffectActionMethods.cpp
    adapters/presentation/EffectParameterMethods.cpp
    adapters/presentation/EffectCompletionMethods.cpp
    adapters/presentation/SoundMethods.cpp
    adapters/presentation/AudioPostBindings.cpp
    adapters/presentation/VoiceResources.cpp
    adapters/presentation/AnimationMoveDataMethods.cpp

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
    src/animation/ActorTiming.cpp
    src/animation/ActorTimingNative.cpp
    src/animation/ActorBgmSync.cpp
    src/animation/ActorBgmSyncNative.cpp
    src/animation/ActorAnimationParameters.cpp
    src/animation/ActionRelations.cpp
    src/animation/AdventureConfig.cpp
    src/animation/AnimationControl.cpp
    src/animation/HitDirection.cpp
    src/animation/AnimationOverrides.cpp
    src/animation/AnimationResources.cpp
    src/animation/AnimatorCatalog.cpp
    src/animation/ControllerBinding.cpp
    src/animation/OfficialCatalog.cpp
    adapters/animation/catalog/VisualCatalog.cpp
    adapters/animation/catalog/PhaseVisualCatalog.cpp
    adapters/animation/catalog/DefinitionCatalog.cpp
    adapters/animation/catalog/CharacterCatalog.cpp
    adapters/animation/catalog/ActionConfigCatalog.cpp
    adapters/animation/catalog/ReadyCatalog.cpp
    adapters/animation/catalog/HitCatalog.cpp
    adapters/animation/catalog/MovementCatalog.cpp
    src/animation/catalog/AbilityCatalog.cpp
    adapters/animation/catalog/NpcCatalog.cpp

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
    src/managed/GenericFactory.cpp
    src/managed/ManagedLists.cpp
    src/managed/ManagedRoots.cpp
    src/managed/References.cpp

    # src/metadata
    src/metadata/MetadataResolver.cpp
    src/metadata/RuntimeMetadata.cpp

    # src/model
    src/model/ModelAccess.cpp
    src/model/ActorModelPresentation.cpp
    src/model/ActorSkinBindings.cpp
    src/model/ActorVisualConfiguration.cpp
    src/model/ActorVisualConfigurationNative.cpp
    src/model/VisualConfigurationHost.cpp
    src/model/ActorModelPresentationNative.cpp
    src/model/ActorModelChange.cpp
    src/model/ActorRendering.cpp
    src/model/ActorRenderingNative.cpp
    src/model/ArtAdmission.cpp
    src/model/CharacterOutfit.cpp
    src/model/CharacterSkin.cpp
    src/model/ModelBaselineCapture.cpp
    src/model/ModelProfile.cpp
    src/model/ModelProfileInputs.cpp
    src/model/ModelProfileRequest.cpp
    src/model/ModelRelease.cpp
    src/model/ModelTransition.cpp
    src/model/ModelServices.cpp
    src/model/ModelScaleProgram.cpp
    src/model/ModelScaleNative.cpp
    src/model/ProfileInputs.cpp

    # src/platform
    src/platform/NativeHooks.cpp

    # src/presentation
    adapters/presentation/PresentationBindings.cpp
    src/presentation/PresentationContext.cpp
    src/presentation/PresentationResources.cpp
    src/presentation/PresentationIsolation.cpp
    src/presentation/PresentationSources.cpp

    # src/presentation/hits
    src/presentation/hits/HitGeometry.cpp
    src/presentation/effects/HitEffectControl.cpp

    # src/presentation/audio
    src/presentation/audio/SoundAccess.cpp
    src/presentation/audio/SoundTracking.cpp
    adapters/presentation/SoundTrackingMethods.cpp
    src/presentation/audio/SoundCallbacks.cpp
    src/presentation/audio/OwnedSoundRequest.cpp
    src/presentation/audio/AudioPostRoute.cpp
    src/presentation/audio/SoundManager.cpp
    src/presentation/audio/ActorAudioHost.cpp

    # src/presentation/movement
    src/presentation/movement/MovementGeometry.cpp
    src/presentation/movement/AnimationMovePlan.cpp
    src/presentation/movement/AnimationMovementData.cpp
    src/presentation/movement/AnimationMovementAccess.cpp
    src/presentation/movement/AnimationMovementUpdates.cpp
    src/presentation/movement/OwnedMovementRequest.cpp
    src/presentation/movement/OwnedMovementHost.cpp
    src/presentation/movement/MovementHostFactory.cpp

    # src/presentation/projectiles
    src/presentation/projectiles/ProjectileInputs.cpp
    src/presentation/projectiles/ProjectileTemplate.cpp
    src/presentation/projectiles/ProjectilePhysics.cpp
    src/presentation/projectiles/OwnedProjectileGroup.cpp
    src/presentation/projectiles/ProjectileWrapperCapture.cpp

    # src/presentation/effects
    src/presentation/effects/EffectLayouts.cpp
    src/presentation/effects/EffectLayoutNative.cpp
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
    src/presentation/effects/EffectViewPolicy.cpp
    src/presentation/effects/EffectSubmissions.cpp
    src/presentation/effects/EntityEffectRetirement.cpp
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
    src/presentation/PresentationCallScope.cpp
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
    adapters/unity/MeshWriterLocator.cpp
    src/unity/OwnedUnityObjects.cpp
    src/unity/RendererAccess.cpp
    src/unity/SceneObjects.cpp
    src/unity/UnityAssetAccess.cpp

    # src/world
    src/world/WorldFrames.cpp
    src/world/WorldMap.cpp
    src/world/WorldPlacement.cpp

    # Domain binding providers
    adapters/entity/LifecycleHookBridge.cpp
    adapters/world/WorldFrameHookBridge.cpp
    adapters/entity/LifecycleBindings.cpp
    adapters/presentation/AnimationEventPolicyBindings.cpp
    adapters/presentation/AnimationExitSchema.cpp
    adapters/presentation/EffectInputBindings.cpp
    adapters/presentation/EffectSchemaBindings.cpp
    adapters/presentation/HitGeometryMethods.cpp
    adapters/presentation/MovementGeometryMethods.cpp
    adapters/presentation/ProjectileGroupBindings.cpp
    adapters/presentation/ProjectileInputBindings.cpp
    adapters/presentation/ProjectilePhysicsBindings.cpp
    adapters/presentation/TimelineBindings.cpp
    adapters/presentation/TimelineRuntimeBindings.cpp
    adapters/animation/AnimationParameterBindings.cpp
    adapters/model/ModelBindings.cpp
    adapters/model/OutfitBindings.cpp
    adapters/model/CharacterSkinBindings.cpp
    adapters/presentation/ContextBindings.cpp
    adapters/presentation/EffectLayoutBindings.cpp
    adapters/presentation/EffectObservationBindings.cpp
    adapters/presentation/EventValueBindings.cpp
    adapters/presentation/FaceMaterialBindings.cpp
    adapters/presentation/MovementHostBindings.cpp
    adapters/presentation/TimelineTaskBindings.cpp
    adapters/animation/BgmSyncBindings.cpp
    adapters/animation/EmotionMethods.cpp
    adapters/animation/HitDirectionMethods.cpp
    adapters/animation/catalog/DataIdentityCatalog.cpp
    adapters/animation/catalog/AbilitySchema.cpp
    adapters/animation/TimingBindings.cpp
    adapters/control/PartnerNativeBindings.cpp
    adapters/entity/EntityRuntimeBindings.cpp
    adapters/entity/NpcInputBindings.cpp
    adapters/model/RenderingBindings.cpp
    adapters/model/ModelPresentationBindings.cpp
    adapters/model/ModelScaleBindings.cpp
    adapters/presentation/AnimationExecutionBindings.cpp
    adapters/presentation/HitEffectMethods.cpp
    adapters/presentation/ProjectileWrapperBindings.cpp
    adapters/unity/MeshWriterCalls.cpp
    adapters/world/WorldFrameMethods.cpp
    adapters/world/WorldPlacementMethods.cpp
)

set(HKSR_API_TEST_SOURCES
    adapters/presentation/TimelineRuntimeBindings.cpp

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
    adapters/unity/MeshWriterLocator.cpp

    # tests/animation
    tests/animation/ActionConfigTests.cpp
    tests/animation/ActionRelationsTests.cpp
    tests/animation/AnimationControlTests.cpp
    tests/animation/HitDirectionTests.cpp
    tests/presentation/HitGeometryTests.cpp
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
    tests/presentation/effects/HitEffectControlTests.cpp
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
