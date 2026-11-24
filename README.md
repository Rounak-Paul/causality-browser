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
- **Windows** uses Visual Studio's C/C++ tools and the Vulkan SDK. Extract the
  `Microsoft.Web.WebView2` NuGet package and pass
  `-DCAUSALITY_WEBVIEW2_SDK_DIR=C:/path/to/extracted/package` at configure
  time. The executable links the WebView2 loader statically. The target computer
  needs the Microsoft Edge WebView2 Runtime, the MSVC runtime, and a
  Vulkan 1.3-capable graphics driver.
- **Linux** uses a C/C++ toolchain and the Vulkan, shaderc, glslang,
  SPIRV-Tools, and GLFW platform development dependencies. Install the
  `pkg-config` development packages for GTK 3 (`gtk+-3.0`) and
  WebKitGTK (`webkit2gtk-4.1` or `webkit2gtk-4.0`). The target computer
  needs the matching libraries, an X11 or XWayland session, and a
  Vulkan 1.3-capable driver.

CMake reports the selected Causality WebView backend at configure time.
If the Windows or Linux SDK is missing at build time, the executable can
open the browser shell and display a WebView availability message.

Copy the executable for the destination's OS and architecture. The app uses
the target system's libraries, GPU driver, and WebView runtime.
