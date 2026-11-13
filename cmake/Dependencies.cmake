include(FetchContent)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../dependencies.json")
file(READ "${CMAKE_CURRENT_LIST_DIR}/../dependencies.json" HKSR_DEPENDENCIES)
foreach(NAME minhook json)
    string(JSON REPOSITORY GET "${HKSR_DEPENDENCIES}" ${NAME} repository)
    string(JSON REVISION GET "${HKSR_DEPENDENCIES}" ${NAME} revision)
    FetchContent_Declare(hksr_${NAME}
        URL "https://codeload.github.com/${REPOSITORY}/tar.gz/${REVISION}"
        TLS_VERIFY TRUE DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR _hksr_no_subproject)
    FetchContent_MakeAvailable(hksr_${NAME})
endforeach()
add_library(hksr_json INTERFACE)
target_include_directories(hksr_json SYSTEM INTERFACE "${hksr_json_SOURCE_DIR}/single_include")
