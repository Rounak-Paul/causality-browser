# causality-browser — project setup

C application, built with CMake, using `vendors/causality` (Vulkan-based
immediate-mode UI library, git submodule) for windowing/rendering,
`vendors/quickjs` (quickjs-ng, git submodule) for JS execution, and
`vendors/gumbo` (gumbo-parser, git submodule) for HTML5 parsing.

**Important gap, not yet built**: causality's CSS parser (`ca_css_parse`)
and gumbo's HTML parser are both real and both integrated, but nothing
yet connects gumbo's DOM tree to causality's `ca_div_begin`/`ca_text`
builder calls. That bridge — walk parsed HTML, resolve CSS against it,
emit causality UI nodes — is unwritten. See "What's NOT covered" at the
bottom of `.context/engine-html-parser.md`.

## Layout
- `CMakeLists.txt` — top-level; adds `vendors/causality`, `vendors/quickjs`
  as subdirectories, `include()`s `cmake/gumbo.cmake` for gumbo, then adds
  `src`. Forces `BUILD_TESTING=OFF` before causality's subdirectory add so
  its test executables aren't built as part of this app; adds quickjs with
  `EXCLUDE_FROM_ALL` for the same reason (its CLI/test/tool targets aren't
  gated by `BUILD_TESTING` upstream, so this is the only lever — the `qjs`
  library target we actually link still builds on demand).
- `cmake/gumbo.cmake` — defines the `gumbo` target by compiling
  `vendors/gumbo/src/*.c` directly (upstream is Meson-only, no CMake).
  Lives in **this project's own tree**, not inside the `vendors/gumbo`
  submodule — a file placed directly inside a submodule's checkout is
  invisible to the parent repo's git history (only the pinned commit SHA
  is tracked via `.gitmodules`), so it would silently disappear on a
  fresh clone elsewhere. See `.context/engine-html-parser.md` for the
  full story (this was the actual bug fixed by moving it here).
- `src/CMakeLists.txt` — builds `causality_browser` executable from `main.c`
  + everything under `src/engine/**.c` (glob, `CONFIGURE_DEPENDS` so new
  engine files are picked up on the next configure without editing this
  file). Links `causality`, `qjs`, and `gumbo` (all PRIVATE); include path
  is `src/` itself, so engine headers are included as `"engine/engine.h"`,
  `"engine/core/..."`, `"engine/js/..."`, `"engine/html/..."`.
- `src/engine/` — reusable, non-browser-domain-specific subsystems. See
  `.context/engine-core-subsystems.md` for logger/event/job-system design,
  `.context/engine-js-runtime.md` for the JS runtime wrapper, and
  `.context/engine-html-parser.md` for the HTML parser wrapper.
  - `engine.h` / `engine.c` — `Eng_Engine`: owns init/shutdown order
    (logger → event bus → job system → JS runtime, reverse on shutdown).
    One call in (`eng_engine_init`), one call out (`eng_engine_shutdown`)
    from `main.c`. Does NOT own HTML documents — those are per-navigation,
    not process-wide singletons; `eng_html_parse`/`_destroy` are called
    directly wherever a document's lifetime is managed (currently just the
    smoke test in `main.c`).
  - `core/logger.{h,c}` — `ENG_LOG_*(tag, fmt, ...)` macros.
  - `core/event.{h,c}` — `Eng_EventBus`, synchronous pub/sub by `Eng_EventId`.
  - `core/job_system.{h,c}` — `Eng_JobSystem`, fixed worker pool + counter-fence joins.
  - `js/js_runtime.{h,c}` — `Eng_JsRuntime`/`Eng_JsContext`, thin wrapper
    over quickjs-ng; raw `JSValue`/`JSContext*`/`JSRuntime*` never cross
    this header.
  - `html/html_parser.{h,c}` — `Eng_HtmlDocument`/`Eng_HtmlNode`, thin
    wrapper over gumbo-parser; raw `GumboOutput*`/`GumboNode*` never cross
    this header.
- `src/main.c` — entry point. Currently: `eng_engine_init` → causality
  instance/window → a startup smoke-test job (exercises job system +
  event bus + logger together) → a JS context eval smoke test → an HTML
  parse + tree-walk smoke test → tick loop (drains pending JS microtasks
  each frame via `eng_js_runtime_run_jobs`) → `ca_instance_destroy` →
  `eng_engine_shutdown`. Base to extend with actual browser UI/logic.
- `src/browser/` — reserved, not yet created. Intended home for
  tab/navigation/network code AND the DOM→causality render bridge once
  that work starts; keeps browser-domain logic out of `engine/`.
- `vendors/causality/` — submodule (nested submodules: glfw, glm, vma,
  freetype+dlg). Init with `git submodule update --init --recursive`.
- `vendors/quickjs/` — submodule, quickjs-ng, pinned to tag `v0.17.0`. MIT
  licensed. Pure C, no nested submodules.
- `vendors/gumbo/` — submodule, gumbo-parser (codeberg.org/gumbo-parser
  fork of Google's archived original — see `.context/engine-html-parser.md`
  for why), pinned to tag `0.14.0`. Apache-2.0. Pure C99, no nested
  submodules. **No CMakeLists.txt inside this submodule** — its CMake
  target is defined externally by `cmake/gumbo.cmake` (see above); keep it
  that way, don't add build files inside `vendors/gumbo/` itself.

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
