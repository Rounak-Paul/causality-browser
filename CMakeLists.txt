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

# quickjs-ng — embedded JS engine. Its own CMakeLists.txt defaults
# QJS_ENABLE_INSTALL to ON (adds install() rules for a standalone quickjs
# package) and BUILD_SHARED_LIBS to OFF (already matches this project's
# static-link convention) — only the install rules need overriding here.
# EXCLUDE_FROM_ALL keeps its unconditional CLI/test/tool executables
# (qjs_exe, qjsc, api-test, lre-test, run-test262, unicode_gen — none of
# them gated by BUILD_TESTING upstream) out of the default build; the
# `qjs` static library target we actually link still builds on demand as
# a real dependency of causality_browser.
set(QJS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(vendors/quickjs EXCLUDE_FROM_ALL)

# gumbo — HTML5 parser. Upstream ships a Meson-only build with no CMake
# support; cmake/gumbo.cmake (our own file, tracked in this repo — not
# inside the vendors/gumbo submodule, see its header comment for why)
# compiles gumbo's library sources directly and defines the `gumbo`
# target.
include(cmake/gumbo.cmake)

add_subdirectory(src)
