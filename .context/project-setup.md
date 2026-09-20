# causality-browser — project setup

C application, built with CMake, using `vendors/causality` (Vulkan-based
immediate-mode UI library, git submodule) for windowing/rendering.

## Layout
- `CMakeLists.txt` — top-level; adds `vendors/causality` then `src`. Forces
  `BUILD_TESTING=OFF` before the subdirectory add so causality's own test
  executables aren't built as part of this app.
- `src/CMakeLists.txt` — builds `causality_browser` executable from `main.c`
  + everything under `src/engine/**.c` (glob, `CONFIGURE_DEPENDS` so new
  engine files are picked up on the next configure without editing this
  file). Links `causality` (PRIVATE); include path is `src/` itself, so
  engine headers are included as `"engine/engine.h"`, `"engine/core/..."`.
- `src/engine/` — reusable, non-browser-domain-specific subsystems. See
  `.context/engine-core-subsystems.md` for logger/event/job-system design.
  - `engine.h` / `engine.c` — `Eng_Engine`: owns init/shutdown order
    (logger → event bus → job system, reverse on shutdown). One call in
    (`eng_engine_init`), one call out (`eng_engine_shutdown`) from `main.c`.
  - `core/logger.{h,c}` — `ENG_LOG_*(tag, fmt, ...)` macros.
  - `core/event.{h,c}` — `Eng_EventBus`, synchronous pub/sub by `Eng_EventId`.
  - `core/job_system.{h,c}` — `Eng_JobSystem`, fixed worker pool + counter-fence joins.
- `src/main.c` — entry point. Currently: `eng_engine_init` → causality
  instance/window → a startup smoke-test job (exercises job system +
  event bus + logger together, see main.c) → tick loop →
  `ca_instance_destroy` → `eng_engine_shutdown`. Base to extend with
  actual browser UI/logic.
- `src/browser/` — reserved, not yet created. Intended home for
  tab/navigation/DOM/network code once that work starts; keeps
  browser-domain logic out of `engine/`.
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

stdout is fully buffered when not attached to a TTY (e.g. `./bin > file.log`
from a script) — logger output is not lost, just delayed until buffer
flush/exit. Use `script -q /dev/null ./build/bin/causality_browser` (or run
in an interactive terminal) to see live logs when testing headlessly.

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
