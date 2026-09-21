# causality-browser — project setup

C application, built with CMake, using `vendors/causality` (Vulkan-based
immediate-mode UI library, git submodule) for windowing/rendering,
`vendors/quickjs` (quickjs-ng, git submodule) for JS execution,
`vendors/gumbo` (gumbo-parser, git submodule) for HTML5 parsing, and
system `libcurl` (found via `find_package`, not vendored — see
`.context/engine-net-fetch.md`) for HTTP(S) fetch.

**Causality is co-evolving with this project, not a frozen dependency.**
`vendors/causality` is the user's own repo (also consumed by a separate
project, Sol) — extend/refactor it in place as the browser's real needs
surface gaps, staying additive/non-breaking for Sol. See
`.context/causality-coevolution.md` for the full mandate before touching
anything under `vendors/causality/`. First real extension already
shipped: `Ca_DivDesc.inline_flow` (real inline text-flow layout),
committed and pushed to the user's causality repo — see
`.context/causality-coevolution.md`'s "Known first gap" section (now
resolved, doc not yet updated to say so) and
`vendors/causality/.context/inline-formatting-context.md`.

The offline DOM→causality render bridge (parse HTML, walk it, emit
`ca_div_begin`/`ca_text` calls styled via causality's own CSS cascade,
including real inline flow and list bullets/indentation) is built — see
`.context/browser-dom-bridge.md`. Network fetch (`engine/net/`) is also
built and verified — see `.context/engine-net-fetch.md`. Still missing
before a live URL can render end-to-end: wiring fetch's result into the
parse/render pipeline (currently separate smoke tests in `main.c`, not
yet connected), linked-stylesheet extraction/fetch, image decoding, JS
DOM bindings. Full list in `browser-dom-bridge.md`'s closing section.

## Layout
- `CMakeLists.txt` — top-level; adds `vendors/causality`, `vendors/quickjs`
  as subdirectories, `include()`s `cmake/gumbo.cmake` for gumbo,
  `find_package(CURL REQUIRED)` for libcurl, then adds `src`. Forces
  `BUILD_TESTING=OFF` before causality's subdirectory add so its test
  executables aren't built as part of this app; adds quickjs with
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
  file). Links `causality`, `qjs`, `gumbo`, and `CURL::libcurl` (all
  PRIVATE); include path is `src/` itself, so engine headers are included
  as `"engine/engine.h"`, `"engine/core/..."`, `"engine/js/..."`,
  `"engine/html/..."`, `"engine/net/..."`.
- `src/engine/` — reusable, non-browser-domain-specific subsystems. See
  `.context/engine-core-subsystems.md` for logger/event/job-system design,
  `.context/engine-js-runtime.md` for the JS runtime wrapper,
  `.context/engine-html-parser.md` for the HTML parser wrapper, and
  `.context/engine-net-fetch.md` for the network fetch wrapper.
  - `engine.h` / `engine.c` — `Eng_Engine`: owns init/shutdown order
    (logger → event bus → job system → JS runtime → net system, reverse
    on shutdown). One call in (`eng_engine_init`), one call out
    (`eng_engine_shutdown`) from `main.c`. Does NOT own HTML documents —
    those are per-navigation, not process-wide singletons;
    `eng_html_parse`/`_destroy` are called directly wherever a document's
    lifetime is managed (currently just the smoke test in `main.c`).
  - `core/logger.{h,c}` — `ENG_LOG_*(tag, fmt, ...)` macros.
  - `core/event.{h,c}` — `Eng_EventBus`, synchronous pub/sub by `Eng_EventId`.
  - `core/job_system.{h,c}` — `Eng_JobSystem`, fixed worker pool + counter-fence joins.
  - `js/js_runtime.{h,c}` — `Eng_JsRuntime`/`Eng_JsContext`, thin wrapper
    over quickjs-ng; raw `JSValue`/`JSContext*`/`JSRuntime*` never cross
    this header.
  - `html/html_parser.{h,c}` — `Eng_HtmlDocument`/`Eng_HtmlNode`, thin
    wrapper over gumbo-parser; raw `GumboOutput*`/`GumboNode*` never cross
    this header.
  - `net/net_fetch.{h,c}` — `Eng_NetSystem`/`eng_net_fetch`, thin wrapper
    over libcurl's easy API; runs on the job system, delivers results via
    callback + optional event. Two real curl-pointer-lifetime bugs were
    caught and fixed here through live execution, not review — see
    `.context/engine-net-fetch.md`'s "Two real bugs" section before
    touching curl option/getinfo handling elsewhere.
- `src/browser/` — browser-domain logic (not reusable engine internals).
  See `.context/browser-dom-bridge.md` for the render bridge design.
  - `dom_bridge.{h,c}` — `eng_dom_render`/`eng_dom_render_builder`: walks
    an `Eng_HtmlDocument` and emits the equivalent causality UI tree,
    including real inline text flow (`Ca_DivDesc.inline_flow`) for
    prose-only blocks and list bullets/indentation for `<ul>`/`<li>`.
    Pure function, no persistent state (causality's builder is
    immediate-mode — re-walks and re-emits every call each time it
    runs). CSS resolution is NOT done here — causality's own
    `ca_div_begin`/etc. already resolve `id`/`style` against the
    attached `Ca_Stylesheet` internally; a small built-in
    `eng_dom_default_css` supplies tag-based styling (bold, link color)
    the browser needs that causality's CSS engine has no type-selector
    equivalent for.
  - Future home for tab/navigation logic, and the fetch→parse→render
    wiring (`eng_net_fetch` → `eng_html_parse` → `eng_dom_render`) once
    that's connected — currently three separate smoke tests in `main.c`,
    not yet chained together.
- `src/main.c` — entry point. Currently: `eng_engine_init` → causality
  instance/window → a startup smoke-test job (exercises job system +
  event bus + logger together) → a JS context eval smoke test → a
  network fetch smoke test (`eng_net_fetch` against a real URL, blocks
  on a counter, logs the result) → an HTML parse + tree-walk smoke test
  → **a real rendered demo page** (parsed CSS attached to the instance,
  parsed HTML rendered via `ca_div_set_builder` +
  `eng_dom_render_builder`) → tick loop (drains pending JS microtasks
  each frame via `eng_js_runtime_run_jobs`) → `ca_instance_destroy` →
  `eng_engine_shutdown`.
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
- **libcurl — NOT vendored, no `vendors/` entry.** Found via
  `find_package(CURL REQUIRED)` against the system install. See
  `.context/engine-net-fetch.md` for why (curl's CMake build has no
  bundled TLS backend option — vendoring it would mean also vendoring
  OpenSSL or similar).

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

This Claude Code sandbox has no external DNS/network egress (confirmed
via plain `curl` from the shell — hangs, then times out on any real
hostname; works fine against `127.0.0.1`). Network-fetch smoke tests
must be verified either against a temporary local server
(`python3 -m http.server` or equivalent — see
`.context/engine-net-fetch.md`'s "Verified" section for the exact
approach used) or on an unrestricted machine. Don't mistake this
sandbox limitation for a code bug — verify a failing fetch produces a
clean, graceful error (it should) before assuming something's broken.

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
