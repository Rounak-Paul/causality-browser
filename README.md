# Causality Browser

A desktop browser foundation built on Causality's native WebView. The
bottom address dock and Start Page open websites or search with DuckDuckGo.
The address field's trailing control switches between Go, Reload, and Stop.
Links that request another window open in a tab.

The Causality title bar and tab panel share a solid theme color. The page
sits inside a rounded frame with a thin edge and slim right and bottom
gutters. The Causality address dock floats over the page on macOS 26 and
later. Other builds place it in a compact bottom lane.

## Tabs

Use **+** on the left tab panel to open a tab. The bottom **›** expands the rail to
show tab titles and group names. **‹** returns to tab icons and folder tiles.
Click a folder to show or hide its tabs.

Drag one tab onto another to group them, or drop a tab onto an existing
folder. In the expanded rail, the folder button next to **Tabs** can create
a named group or move the active tab. A group's **+** opens a tab inside it,
and **×** removes the group while keeping its tabs. An individual tab's **×**
closes that tab. Tabs and groups are kept for the current session.

## Appearance

Open the **…** browser menu to return to the Start Page, copy the current
address, or change page zoom. Under **Appearance**, choose **Dark**,
**Light**, or **Dusk**. The palette applies immediately to the browser chrome
and is saved for your next launch. Websites render their own styles.

## Build

From the repository root, with CMake 3.22 or newer and a C/C++ toolchain:

```sh
git submodule update --init --recursive
cmake -S . -B build
cmake --build build --config Release --parallel
```

The Release executable goes to `bin/causality-browser` on macOS and Linux, or
`bin/causality-browser.exe` on Windows. Intermediate artifacts go to the
configured build directory. The executable accepts an optional initial HTTP
or HTTPS address as its only launch argument.

- **macOS** uses Xcode's command-line toolchain and the Vulkan SDK. Set
  `VULKAN_SDK` to the SDK's macOS directory if CMake cannot find it. MoltenVK
  is linked statically and the browser uses the OS's WebKit framework.
  For one binary with both Mac architectures, pass
  `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` at configure time. The default
  deployment target is macOS 11, subject to the GPU and Vulkan requirements.
- **Windows** uses Visual Studio's C/C++ tools and the Vulkan SDK. The target
  computer needs a Vulkan 1.3-capable graphics driver and the MSVC runtime.
- **Linux** uses a C/C++ toolchain and the Vulkan, shaderc, glslang,
  SPIRV-Tools, and GLFW platform development dependencies. Run on a
  compatible Linux distribution with its system libraries, display stack, and
  a Vulkan 1.3-capable driver.

Copy the executable for the destination's OS and architecture. Browser assets
and project libraries are linked into it. The executable uses the destination
OS's libraries and the GPU driver.

## WebView support

The pinned Causality revision implements WebView on macOS. On Windows and Linux
the browser shell displays a message when the native WebView is unavailable.
After updating the Causality submodule, rerun the build. The browser queries
Causality at runtime, so a backend added upstream uses the same application code.
