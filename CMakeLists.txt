cmake_minimum_required(VERSION 3.25)
project(HksrApi VERSION 4.7.0 LANGUAGES C CXX RC)
set(HKSR_API_INTERFACE_GENERATION 4)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "HksrApi requires an x64 MSVC-compatible Windows toolchain")
endif()

# The native package shares C++ values with its consumers. Keep the binary's
# CRT explicit; the exported check rejects incompatible consumer settings.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
configure_file(cmake/CppAbi.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/CppAbi.hpp.in" @ONLY)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/abi/$<CONFIG>/hksr/CppAbi.hpp"
    INPUT "${CMAKE_CURRENT_BINARY_DIR}/CppAbi.hpp.in")

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(cmake/Sources.cmake)
include(cmake/Dependencies.cmake)

# Shared native primitives, not a second game runtime or remote invocation API.
# Their separate include root prevents ordinary Api consumers from acquiring
# candidate-table and native invocation access through transitive includes.
add_library(HksrApiNative STATIC native/src/MetadataReader.cpp
    native/src/ObjectAccess.cpp native/src/LoadedModule.cpp adapters/runtime/UnityMetadataLocator.cpp)
add_library(Hksr::NativeSupport ALIAS HksrApiNative)
set_target_properties(HksrApiNative PROPERTIES EXPORT_NAME NativeSupport)
target_compile_features(HksrApiNative PUBLIC cxx_std_17)
target_compile_options(HksrApiNative PRIVATE /W4 /WX /utf-8)
target_compile_definitions(HksrApiNative PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(HksrApiNative PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/abi/$<CONFIG>>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/native/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/HksrApiNative>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_options(HksrApiNative PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:/FIhksr/CppAbi.hpp>")

add_library(hksr_minhook STATIC
    "${hksr_minhook_SOURCE_DIR}/src/buffer.c"
    "${hksr_minhook_SOURCE_DIR}/src/hook.c"
    "${hksr_minhook_SOURCE_DIR}/src/trampoline.c"
    "${hksr_minhook_SOURCE_DIR}/src/hde/hde64.c")
target_include_directories(hksr_minhook PUBLIC "${hksr_minhook_SOURCE_DIR}/include")

configure_file(cmake/Version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/Version.rc" @ONLY)
add_library(HksrApi SHARED ${HKSR_API_SOURCES} "${CMAKE_CURRENT_BINARY_DIR}/Version.rc")
add_library(Hksr::Api ALIAS HksrApi)
set_target_properties(HksrApi PROPERTIES OUTPUT_NAME Hksr.Api EXPORT_NAME Api)
target_compile_features(HksrApi PUBLIC cxx_std_17)
target_compile_definitions(HksrApi PRIVATE HKSR_API_BUILD WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(HksrApi PRIVATE /W4 /WX /utf-8)
target_include_directories(HksrApi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/abi/$<CONFIG>>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_options(HksrApi PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:/FIhksr/CppAbi.hpp>")
target_link_libraries(HksrApi PRIVATE hksr_minhook HksrApiNative hksr_json)

if(BUILD_TESTING)
    add_library(AbiConsumer SHARED tests/package/AbiConsumer.cpp)
    target_compile_options(AbiConsumer PRIVATE /W4 /WX /utf-8)
    target_link_libraries(AbiConsumer PRIVATE HksrApi)
    add_executable(AbiCapabilitiesTests tests/package/AbiCapabilitiesTests.cpp)
    target_compile_options(AbiCapabilitiesTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(AbiCapabilitiesTests PRIVATE HksrApi)
    add_dependencies(AbiCapabilitiesTests AbiConsumer)
    add_test(NAME sdk.interface-capabilities COMMAND AbiCapabilitiesTests "$<TARGET_FILE:AbiConsumer>")
    add_library(IncompatibleSdk SHARED tests/package/IncompatibleSdk.cpp)
    target_include_directories(IncompatibleSdk PRIVATE include)
    target_compile_features(IncompatibleSdk PRIVATE cxx_std_17)
    set_target_properties(IncompatibleSdk PROPERTIES OUTPUT_NAME Hksr.Api
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/incompatible-interface"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/incompatible-interface")
    add_executable(AbiLoadTests tests/package/AbiLoadTests.cpp)
    add_dependencies(AbiLoadTests AbiConsumer IncompatibleSdk)
    add_test(NAME sdk.interface-loading COMMAND AbiLoadTests "$<TARGET_FILE:AbiConsumer>"
        "$<TARGET_FILE:HksrApi>" "$<TARGET_FILE:IncompatibleSdk>")
    install(TARGETS AbiConsumer AbiLoadTests AbiCapabilitiesTests RUNTIME DESTINATION checks/abi${HKSR_API_INTERFACE_GENERATION})
    add_library(ContextConsumerCompile OBJECT tests/presentation/ContextConsumerCompile.cpp)
    target_compile_options(ContextConsumerCompile PRIVATE /W4 /WX /utf-8)
    target_link_libraries(ContextConsumerCompile PRIVATE HksrApi)
    add_executable(PresentationClientsTests tests/presentation/PresentationClientsTests.cpp)
    target_compile_options(PresentationClientsTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(PresentationClientsTests PRIVATE HksrApi)
    add_test(NAME sdk.presentation-clients COMMAND PresentationClientsTests)
    add_executable(PresentationReadsTests tests/presentation/PresentationReadsTests.cpp)
    target_compile_options(PresentationReadsTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(PresentationReadsTests PRIVATE HksrApi)
    add_test(NAME sdk.presentation-reads COMMAND PresentationReadsTests)
    add_executable(FaceMaterialComponentsTests tests/presentation/FaceMaterialComponentsTests.cpp)
    target_compile_options(FaceMaterialComponentsTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(FaceMaterialComponentsTests PRIVATE HksrApi)
    add_test(NAME sdk.face-material-components COMMAND FaceMaterialComponentsTests)
    add_executable(AnimationParameterTests tests/animation/AnimationParameterTests.cpp)
    target_compile_options(AnimationParameterTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(AnimationParameterTests PRIVATE HksrApi HksrApiNative)
    add_test(NAME sdk.animation-parameters COMMAND AnimationParameterTests)
    add_executable(ActorModelPresentationTests tests/model/ActorModelPresentationTests.cpp)
    add_executable(ActorVisualConfigurationTests tests/model/ActorVisualConfigurationTests.cpp
        src/model/VisualConfigurationHost.cpp adapters/model/ModelBindings.cpp
        adapters/model/VisualConfigurationBindings.cpp)
    target_compile_options(ActorVisualConfigurationTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(ActorVisualConfigurationTests PRIVATE HksrApi)
    add_test(NAME sdk.actor-visual-configuration COMMAND ActorVisualConfigurationTests)
    target_compile_options(ActorModelPresentationTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(ActorModelPresentationTests PRIVATE HksrApi HksrApiNative)
    add_test(NAME sdk.model-presentation COMMAND ActorModelPresentationTests)
    add_executable(ActorModelChangeTests tests/model/ActorModelChangeTests.cpp)
    target_compile_options(ActorModelChangeTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(ActorModelChangeTests PRIVATE HksrApi)
    add_test(NAME sdk.actor-model-change COMMAND ActorModelChangeTests)
    add_executable(ActorTimingTests tests/animation/ActorTimingTests.cpp tests/animation/ActorTimingNativeTests.cpp)
    target_compile_options(ActorTimingTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(ActorTimingTests PRIVATE HksrApi)
    add_test(NAME sdk.actor-timing COMMAND ActorTimingTests)
    add_executable(ActorBgmSyncTests tests/animation/ActorBgmSyncTests.cpp)
    target_compile_options(ActorBgmSyncTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(ActorBgmSyncTests PRIVATE HksrApi HksrApiNative)
    add_test(NAME sdk.actor-bgm-sync COMMAND ActorBgmSyncTests)
    add_executable(ActorRenderingTests tests/model/ActorRenderingTests.cpp adapters/model/RenderingBindings.cpp)
    target_compile_options(ActorRenderingTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(ActorRenderingTests PRIVATE HksrApi HksrApiNative)
    add_test(NAME sdk.actor-rendering COMMAND ActorRenderingTests)
    add_executable(TimelineTaskBindingsTests tests/presentation/timeline/TimelineTaskBindingsTests.cpp
        adapters/presentation/TimelineTaskBindings.cpp adapters/presentation/TimelineRuntimeBindings.cpp
        adapters/presentation/EffectObservationBindings.cpp adapters/presentation/AnimationExecutionBindings.cpp)
    target_compile_options(TimelineTaskBindingsTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(TimelineTaskBindingsTests PRIVATE HksrApi HksrApiNative)
    add_test(NAME sdk.timeline-task-bindings COMMAND TimelineTaskBindingsTests)
    add_executable(EffectLayoutTests tests/presentation/effects/EffectLayoutTests.cpp)
    target_compile_options(EffectLayoutTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(EffectLayoutTests PRIVATE HksrApi HksrApiNative)
    add_test(NAME sdk.effect-layout COMMAND EffectLayoutTests)
    add_executable(PartnerControlTests tests/control/PartnerControlSessionTests.cpp)
    target_compile_options(PartnerControlTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(PartnerControlTests PRIVATE HksrApi)
    add_test(NAME sdk.partner-controls COMMAND PartnerControlTests)
    add_executable(PartnerEventCleanupTests tests/entity/PartnerEventCleanupTests.cpp)
    target_compile_options(PartnerEventCleanupTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(PartnerEventCleanupTests PRIVATE HksrApi)
    add_test(NAME sdk.partner-event-cleanup COMMAND PartnerEventCleanupTests)
    add_executable(EffectSubmissionsTests tests/presentation/effects/EffectSubmissionsTests.cpp)
    target_compile_options(EffectSubmissionsTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(EffectSubmissionsTests PRIVATE HksrApi)
    add_test(NAME sdk.effect-submissions COMMAND EffectSubmissionsTests)
    add_executable(SoundRequestTests tests/presentation/audio/SoundRequestTests.cpp)
    add_executable(AudioPostRouteTests tests/presentation/audio/AudioPostRouteTests.cpp)
    target_compile_options(AudioPostRouteTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(AudioPostRouteTests PRIVATE HksrApi)
    add_test(NAME sdk.audio-post-route COMMAND AudioPostRouteTests)
    add_executable(ActorAudioSourceTests tests/presentation/audio/ActorAudioSourceTests.cpp adapters/presentation/AudioPostBindings.cpp)
    target_compile_definitions(ActorAudioSourceTests PRIVATE HKSR_API_BUILD)
    target_include_directories(ActorAudioSourceTests PRIVATE include)
    target_compile_options(ActorAudioSourceTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(ActorAudioSourceTests PRIVATE HksrApi HksrApiNative)
    add_test(NAME sdk.actor-audio-source COMMAND ActorAudioSourceTests)
    add_executable(VoiceResourcesTests tests/presentation/audio/VoiceResourcesTests.cpp adapters/presentation/VoiceResources.cpp)
    target_compile_definitions(VoiceResourcesTests PRIVATE HKSR_API_BUILD)
    target_include_directories(VoiceResourcesTests PRIVATE include)
    target_compile_options(VoiceResourcesTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(VoiceResourcesTests PRIVATE HksrApi HksrApiNative)
    add_test(NAME sdk.voice-resources COMMAND VoiceResourcesTests)
    add_executable(AttachmentVisibilityTests tests/presentation/AttachmentVisibilityTests.cpp)
    target_compile_options(AttachmentVisibilityTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(AttachmentVisibilityTests PRIVATE HksrApi)
    add_test(NAME sdk.attachment-visibility COMMAND AttachmentVisibilityTests)
    add_executable(AttachmentVisibilityNativeTests tests/presentation/AttachmentVisibilityNativeTests.cpp
        src/presentation/attachments/AttachmentVisibilityNative.cpp)
    target_compile_options(AttachmentVisibilityNativeTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(AttachmentVisibilityNativeTests PRIVATE HksrApi HksrApiNative)
    add_test(NAME sdk.attachment-visibility-native COMMAND AttachmentVisibilityNativeTests)
    add_executable(AttachmentScaleTests tests/presentation/AttachmentScaleTests.cpp)
    target_compile_options(AttachmentScaleTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(AttachmentScaleTests PRIVATE HksrApi)
    add_test(NAME sdk.attachment-scale COMMAND AttachmentScaleTests)
    add_executable(AttachmentScaleNativeTests tests/presentation/AttachmentScaleNativeTests.cpp
        src/presentation/attachments/AttachmentScaleNative.cpp)
    target_compile_options(AttachmentScaleNativeTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(AttachmentScaleNativeTests PRIVATE HksrApi HksrApiNative)
    add_test(NAME sdk.attachment-scale-native COMMAND AttachmentScaleNativeTests)
    target_compile_options(SoundRequestTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(SoundRequestTests PRIVATE HksrApi)
    add_test(NAME sdk.sound-requests COMMAND SoundRequestTests)
    add_executable(AnimationMovementDataTests tests/presentation/movement/AnimationMovementDataTests.cpp)
    target_compile_options(AnimationMovementDataTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(AnimationMovementDataTests PRIVATE HksrApi)
    add_test(NAME sdk.movement-data COMMAND AnimationMovementDataTests)
    add_executable(MovementHostTests tests/presentation/movement/MovementHostTests.cpp)
    target_compile_options(MovementHostTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(MovementHostTests PRIVATE HksrApi)
    add_test(NAME sdk.movement-host COMMAND MovementHostTests)
    add_executable(ProjectileTests tests/presentation/projectiles/ProjectileTests.cpp)
    target_compile_options(ProjectileTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(ProjectileTests PRIVATE HksrApi)
    add_test(NAME sdk.projectiles COMMAND ProjectileTests)
    add_executable(AnimationSequenceExecutionsTests tests/presentation/events/AnimationSequenceExecutionsTests.cpp)
    target_compile_options(AnimationSequenceExecutionsTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(AnimationSequenceExecutionsTests PRIVATE HksrApi)
    add_test(NAME sdk.sequence-executions COMMAND AnimationSequenceExecutionsTests)
    add_executable(AnimationEventDefinitionsTests tests/presentation/events/AnimationEventDefinitionsTests.cpp)
    target_compile_options(AnimationEventDefinitionsTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(AnimationEventDefinitionsTests PRIVATE HksrApi)
    add_test(NAME sdk.event-definitions COMMAND AnimationEventDefinitionsTests)
    add_test(NAME sdk.module-boundaries COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckBoundaries.cmake")
    add_executable(MetadataCacheTests tests/metadata/MetadataCacheTests.cpp)
    target_include_directories(MetadataCacheTests PRIVATE include)
    target_compile_features(MetadataCacheTests PRIVATE cxx_std_17)
    target_compile_options(MetadataCacheTests PRIVATE /W4 /WX /utf-8)
    add_test(NAME sdk.metadata-cache COMMAND MetadataCacheTests)
    set_tests_properties(sdk.metadata-cache PROPERTIES TIMEOUT 10)
    add_executable(RuntimeClientsTests tests/runtime/RuntimeClientsTests.cpp)
    target_compile_options(RuntimeClientsTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(RuntimeClientsTests PRIVATE HksrApi)
    add_test(NAME sdk.runtime-clients COMMAND RuntimeClientsTests)
    add_executable(NativeSupportTests native/tests/NativeSupportTests.cpp)
    target_compile_options(NativeSupportTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(NativeSupportTests PRIVATE HksrApiNative)
    add_test(NAME sdk.native-support COMMAND NativeSupportTests)
    add_executable(ActorInputPolicyTests tests/model/ActorInputPolicyTests.cpp)
    target_compile_options(ActorInputPolicyTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(ActorInputPolicyTests PRIVATE HksrApi HksrApiNative)
    add_test(NAME sdk.actor-input-policy COMMAND ActorInputPolicyTests)
    add_executable(OperationTraceTests tests/runtime/OperationTraceTests.cpp)
    target_compile_options(OperationTraceTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(OperationTraceTests PRIVATE HksrApi)
    add_test(NAME sdk.operation-trace COMMAND OperationTraceTests)
    add_executable(CharacterSkinTests tests/model/CharacterSkinTests.cpp src/model/CharacterSkin.cpp)
    target_compile_definitions(CharacterSkinTests PRIVATE HKSR_API_BUILD)
    target_include_directories(CharacterSkinTests PRIVATE include)
    target_compile_features(CharacterSkinTests PRIVATE cxx_std_17)
    target_compile_options(CharacterSkinTests PRIVATE /W4 /WX /utf-8)
    add_test(NAME sdk.character-skin COMMAND CharacterSkinTests)
    add_executable(CharacterSkinBindingsTests tests/model/CharacterSkinBindingsTests.cpp
        src/model/CharacterSkin.cpp src/model/ActorSkinBindings.cpp adapters/model/CharacterSkinBindings.cpp)
    target_compile_definitions(CharacterSkinBindingsTests PRIVATE HKSR_API_BUILD)
    target_include_directories(CharacterSkinBindingsTests PRIVATE include)
    target_compile_features(CharacterSkinBindingsTests PRIVATE cxx_std_17)
    target_compile_options(CharacterSkinBindingsTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(CharacterSkinBindingsTests PRIVATE HksrApi HksrApiNative)
    add_test(NAME sdk.character-skin-bindings COMMAND CharacterSkinBindingsTests)
    add_executable(HksrApiTests ${HKSR_API_TEST_SOURCES})
    target_compile_options(HksrApiTests PRIVATE /W4 /WX /utf-8)
    target_link_libraries(HksrApiTests PRIVATE HksrApi HksrApiNative hksr_json)
    add_test(NAME sdk.boundaries COMMAND HksrApiTests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/unity-asset-methods.tsv")
    add_test(NAME sdk.installed-consumer COMMAND "${CMAKE_COMMAND}"
        "-DSDK_BUILD=${CMAKE_CURRENT_BINARY_DIR}" "-DSDK_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/package-test" "-DCONFIG=$<CONFIG>"
        "-DGENERATOR=${CMAKE_GENERATOR}" "-DINTERFACE_GENERATION=${HKSR_API_INTERFACE_GENERATION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/TestInstalledConsumer.cmake")
    set_tests_properties(sdk.installed-consumer PROPERTIES TIMEOUT 120)
endif()

install(TARGETS HksrApi EXPORT HksrApiTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/abi/$<CONFIG>/hksr/CppAbi.hpp"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hksr)
install(TARGETS HksrApiNative EXPORT HksrApiNativeTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY native/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/HksrApiNative)
install(EXPORT HksrApiNativeTargets NAMESPACE Hksr:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HksrApiNative)
configure_package_config_file(cmake/HksrApiNativeConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/HksrApiNativeConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HksrApiNative)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/HksrApiNativeConfigVersion.cmake"
    COMPATIBILITY ExactVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/HksrApiNativeConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/HksrApiNativeConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HksrApiNative)
install(FILES "${hksr_minhook_SOURCE_DIR}/LICENSE.txt" DESTINATION ${CMAKE_INSTALL_DATADIR}/HksrApi/licenses/minhook)
install(FILES "${hksr_json_SOURCE_DIR}/LICENSE.MIT" DESTINATION ${CMAKE_INSTALL_DATADIR}/HksrApi/licenses/nlohmann)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/HksrApi/licenses)
install(FILES README.md BUILDING.md CONTRIBUTING.md LICENSE dependencies.json DESTINATION ${CMAKE_INSTALL_DATADIR}/HksrApi)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DATADIR}/HksrApi/docs
    FILES_MATCHING PATTERN "*.md")
install(EXPORT HksrApiTargets NAMESPACE Hksr:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HksrApi)
configure_package_config_file(cmake/HksrApiConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/HksrApiConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HksrApi)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/HksrApiConfigVersion.cmake"
    COMPATIBILITY AnyNewerVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/HksrApiConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/HksrApiConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HksrApi)
include(cmake/Distribution.cmake)
