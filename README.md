# Causality Browser

A desktop browser foundation built on Causality's native WebView. The window
has an address bar, back and forward navigation, and reload and stop controls.
Links that request another window open in the current view.

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
