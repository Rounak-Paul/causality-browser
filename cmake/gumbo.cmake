# SPDX-License-Identifier: Apache-2.0
#
# CMake build for gumbo-parser (https://codeberg.org/gumbo-parser/gumbo-parser),
# written by this project rather than upstream — gumbo ships a Meson build
# only. All files gumbo's own meson.build lists as its library sources are
# plain C99 with generated tables (tag_enum.h, tag_gperf.h, char_ref_gperf.c)
# already checked into the repo, so no codegen step is needed at build time;
# compiling them directly here mirrors how causality's own CMakeLists.txt
# vendors freetype/glfw/vma without pulling in their upstream build systems.
#
# Lives here (this project's own tree) rather than inside vendors/gumbo/
# itself: a file written inside a git submodule's working directory is
# invisible to the parent repo's history — only vendors/gumbo's pinned
# commit SHA is tracked via .gitmodules, so anything placed directly in
# that checkout would silently disappear on a fresh clone or checkout on
# another machine. Defining the target here, pointed at
# vendors/gumbo/src/*.c from outside, keeps this file tracked and
# reproducible everywhere the repo is cloned.

set(GUMBO_SRC_DIR ${CMAKE_SOURCE_DIR}/vendors/gumbo/src)

add_library(gumbo STATIC
    ${GUMBO_SRC_DIR}/attribute.c
    ${GUMBO_SRC_DIR}/char_ref.c
    ${GUMBO_SRC_DIR}/char_ref_gperf.c
    ${GUMBO_SRC_DIR}/error.c
    ${GUMBO_SRC_DIR}/parser.c
    ${GUMBO_SRC_DIR}/string_buffer.c
    ${GUMBO_SRC_DIR}/string_piece.c
    ${GUMBO_SRC_DIR}/tag.c
    ${GUMBO_SRC_DIR}/tokenizer.c
    ${GUMBO_SRC_DIR}/utf8.c
    ${GUMBO_SRC_DIR}/util.c
    ${GUMBO_SRC_DIR}/vector.c
)

set_target_properties(gumbo PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

# GUMBO_SRC_DIR is PUBLIC: gumbo.h is the only public header, but it
# #includes tag_enum.h directly from the same directory, so consumers need
# this path even though they never include tag_enum.h themselves.
target_include_directories(gumbo PUBLIC
    ${GUMBO_SRC_DIR}
)

if(NOT WIN32)
    target_compile_options(gumbo PRIVATE -fvisibility=hidden)
endif()
