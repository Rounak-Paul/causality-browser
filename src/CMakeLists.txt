add_executable(causality-browser
    main.c
    appearance/theme.c
    appearance/theme_preferences.c
    browser/address.c
    browser/tabs.c
    browser/window.c
)

target_link_libraries(causality-browser PRIVATE causality)
target_compile_definitions(causality-browser PRIVATE BROWSER_VERSION="${PROJECT_VERSION}")

if(APPLE)
    target_sources(causality-browser PRIVATE chrome/page_overlay_mac.m)
    set_source_files_properties(chrome/page_overlay_mac.m PROPERTIES COMPILE_OPTIONS -fobjc-arc)
    target_include_directories(causality-browser SYSTEM PRIVATE "${Vulkan_INCLUDE_DIR}")
    target_link_libraries(causality-browser PRIVATE glfw
        "-framework AppKit"
        "-framework WebKit"
        "-framework CoreFoundation"
        "-framework Foundation"
        "-framework Metal"
        "-framework QuartzCore"
        "-framework CoreGraphics"
        "-framework IOSurface"
        "-framework IOKit"
    )
elseif(WIN32)
    target_link_libraries(causality-browser PRIVATE advapi32)
    # Causality's public headers otherwise declare DLL imports on Windows.
    target_compile_definitions(causality-browser PRIVATE causality_EXPORTS)
    if(MSVC)
        # Match the CRT used by Causality and the Vulkan SDK's shaderc archive.
        set_property(TARGET causality-browser PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    endif()
endif()

if(NOT APPLE)
    target_sources(causality-browser PRIVATE chrome/page_overlay_stub.c)
endif()

# A generator expression suppresses the configuration suffix of multi-config generators.
set_target_properties(causality-browser PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "$<1:${PROJECT_SOURCE_DIR}/bin>"
)
