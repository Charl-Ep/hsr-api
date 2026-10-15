cmake_minimum_required(VERSION 3.25)
project(HksrApi VERSION 4.6.2 LANGUAGES C CXX RC)
set(HKSR_API_INTERFACE_GENERATION 1)

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
    native/src/ObjectAccess.cpp native/src/LoadedModule.cpp native/src/UnityMetadataLocator.cpp)
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
install(FILES README.md BUILDING.md CONTRIBUTING.md LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/HksrApi)
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
