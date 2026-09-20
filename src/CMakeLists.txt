file(GLOB_RECURSE CAUSALITY_BROWSER_ENGINE_SOURCES CONFIGURE_DEPENDS engine/*.c)

add_executable(causality_browser
    main.c
    ${CAUSALITY_BROWSER_ENGINE_SOURCES}
)

target_include_directories(causality_browser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(causality_browser PRIVATE causality qjs gumbo)
