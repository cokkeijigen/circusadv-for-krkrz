cmake_minimum_required(VERSION 3.22.1)

# Check if VCPKG_ROOT is passed as a variable or environment variable
if(NOT VCPKG_ROOT AND DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_ROOT $ENV{VCPKG_ROOT})
endif()

if(VCPKG_ROOT)
    include("${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()

if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(VCPKG_TARGET_TRIPLET "arm64-android" CACHE STRING "")
elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(VCPKG_TARGET_TRIPLET "arm-android" CACHE STRING "")
elseif(ANDROID_ABI STREQUAL "x86_64")
    set(VCPKG_TARGET_TRIPLET "x64-android" CACHE STRING "")
endif()

# Explicitly enable C and CXX to ensure C_COMPILER is initialized
project("circusadv" C CXX)

set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(KR2ANDROID_DIR "${CMAKE_CURRENT_LIST_DIR}/kr2android")
if(NOT EXISTS "${KR2ANDROID_DIR}/CMakeLists.txt")
    include(FetchContent)
    FetchContent_Declare(
            kr2android
            GIT_REPOSITORY https://github.com/cokkeijigen/Kirikiroid2.git
            GIT_TAG kr2android
            SOURCE_DIR ${KR2ANDROID_DIR}
    )
    FetchContent_MakeAvailable(kr2android)
else()
    add_subdirectory(${KR2ANDROID_DIR})
endif()

find_package(ZLIB REQUIRED)

if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(PLUGIN_OUTPUT_NAME "${CMAKE_PROJECT_NAME}.plugin.arm64-v8a")
elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(PLUGIN_OUTPUT_NAME "${CMAKE_PROJECT_NAME}.plugin.armeabi-v7a")
endif()

add_library(${CMAKE_PROJECT_NAME} SHARED
        src/native-lib.cpp
        src/utils/image_crx.cpp
)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
        OUTPUT_NAME "${PLUGIN_OUTPUT_NAME}"
)

target_link_libraries(${CMAKE_PROJECT_NAME}
        android
        log
        kr2android
        ZLIB::ZLIB
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        src
        src/utils
)

target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
        -Wno-braced-scalar-init
)