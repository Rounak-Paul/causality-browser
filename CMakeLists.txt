cmake_minimum_required(VERSION 3.20)

project(causality_browser LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Causality is buildable standalone; BUILD_TESTING gates its own test
# executables (see vendors/causality/causality/CMakeLists.txt) — off here
# since this project only consumes the library.
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

# Output directories — applies to this project's own targets, and to every
# vendored subdirectory added below (causality only self-applies this when
# it is the top-level CMAKE_SOURCE_DIR, which it isn't here).
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(vendors/causality)

add_subdirectory(src)
