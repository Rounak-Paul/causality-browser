# causality-browser — project setup

C application, built with CMake, using `vendors/causality` (Vulkan-based
immediate-mode UI library, git submodule) for windowing/rendering.

## Layout
- `CMakeLists.txt` — top-level; adds `vendors/causality` then `src`. Forces
  `BUILD_TESTING=OFF` before the subdirectory add so causality's own test
  executables aren't built as part of this app.
- `src/CMakeLists.txt` — builds `causality_browser` executable from `main.c`,
  links `causality` (PRIVATE).
- `src/main.c` — entry point. Currently: `ca_instance_create` →
  `ca_window_create` → tick loop → `ca_instance_destroy`. This is the base to
  extend with actual browser UI/logic.
- `vendors/causality/` — submodule (nested submodules: glfw, glm, vma,
  freetype+dlg). Init with `git submodule update --init --recursive`.

## Build
```
cmake -B build -S .
cmake --build build -j
```
Binary lands at `build/bin/causality_browser`, static libs (causality,
freetype, glfw) at `build/lib/`. The root `CMakeLists.txt` sets
`CMAKE_RUNTIME/LIBRARY/ARCHIVE_OUTPUT_DIRECTORY` itself before adding
`vendors/causality` — causality's own CMakeLists.txt only does this
redirection when it is the top-level `CMAKE_SOURCE_DIR`, which it isn't
here, so this project sets it once at the root and every subdirectory
target (including vendored ones) inherits it.

Verified working on macOS (AppleClang, system Vulkan SDK at `/usr/local`,
libvulkan 1.4.328): configures and links cleanly, binary launches and exits
without crashing. Vendor code (VMA) emits ~900 harmless `-Wnullability-
completeness` warnings on Apple Clang — not actionable, not our code.

## Key causality API surface (see vendors/causality/causality/include/causality.h)
- `Ca_InstanceDesc` / `ca_instance_create` / `ca_instance_destroy` — one per app.
- `Ca_WindowDesc` / `ca_window_create` — supports multiple windows per instance.
- `ca_instance_tick(instance)` — pumps events + renders one frame; returns
  false once all windows are closed (drives the main loop).
- Declarative immediate-mode UI: `ca_ui_begin/end`, `ca_div_begin/end`,
  `ca_text`, `ca_btn_begin/end`, CSS via `ca_css_parse` +
  `ca_instance_set_stylesheet`. Full widget set (checkbox, slider, tabs,
  tree, table, splitter, modal, tooltip, viewport) in `ca_components.h`.
- Reference implementation: `vendors/causality/sandbox/src/main.c` — full
  feature tour, good pattern source for widgets not yet used here.
