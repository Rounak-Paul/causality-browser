find_path(Vulkan_INCLUDE_DIR vulkan/vulkan.h
    HINTS "$ENV{VULKAN_SDK}/include" REQUIRED)
get_filename_component(browser_vulkan_root "${Vulkan_INCLUDE_DIR}" DIRECTORY)

find_file(BROWSER_MOLTENVK_ARCHIVE
    NAMES libMoltenVK.a
    HINTS "${browser_vulkan_root}/lib" "$ENV{VULKAN_SDK}/lib" /usr/local/lib /opt/homebrew/lib
    PATH_SUFFIXES MoltenVK.xcframework/macos-arm64_x86_64
    DOC "Static macOS MoltenVK archive from the Vulkan SDK"
    REQUIRED
)

# Causality uses FindVulkan. Feed it the static implementation so both
# Causality and GLFW can run without a separately installed Vulkan loader.
set(Vulkan_LIBRARY "${BROWSER_MOLTENVK_ARCHIVE}")
