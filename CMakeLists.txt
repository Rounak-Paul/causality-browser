cmake_minimum_required(VERSION 3.22)

if(CMAKE_HOST_APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET AND
   NOT DEFINED ENV{MACOSX_DEPLOYMENT_TARGET})
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

project(CausalityBrowser VERSION 0.1.0 LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Darwin|Windows|Linux)$")
    message(FATAL_ERROR "Causality Browser supports macOS, Windows and Linux desktop builds.")
endif()

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING OFF)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendors/causality/CMakeLists.txt")
    message(FATAL_ERROR "Initialize Causality with git submodule update --init --recursive.")
endif()

if(APPLE)
    enable_language(OBJC)
    # HVF adds a dependency on libhvf, which is absent on older macOS releases.
    set(FT_DISABLE_HVF ON)
    include(cmake/StaticMoltenVK.cmake)
endif()

add_subdirectory(vendors/causality EXCLUDE_FROM_ALL)
add_subdirectory(src)
