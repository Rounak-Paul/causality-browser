# engine/html — gumbo-parser wrapper

`src/engine/html/html_parser.{h,c}` wraps gumbo-parser (`vendors/gumbo`,
pinned tag `0.14.0`, Apache-2.0). See `.context/project-setup.md` for
overall layout. Same discipline as `engine/core` and `engine/js`: raw
vendor types (`GumboOutput*`, `GumboNode*`, `GumboAttribute*`) never cross
the header — callers only see `Eng_HtmlDocument`, `Eng_HtmlNode`,
`Eng_HtmlAttribute`.

## Why gumbo-parser (codeberg fork) over alternatives
Google's original `google/gumbo-parser` is **archived**; its own README
explicitly redirects to `codeberg.org/gumbo-parser/gumbo-parser` as the
maintained successor (verified active — commits within the last month as
of this writing). Considered and rejected:
- **lexbor**: also active, pure C, broader scope (bundles its own CSS
  selector engine + encoding detection) — but that overlaps with
  causality's own CSS parser, and we don't want two competing CSS
  implementations in the tree. Gumbo does one job (HTML → tree) and stops.
- **libxml2's HTMLparser**: lenient but not a spec-accurate HTML5
  tree-construction implementation (no proper tag-soup error recovery per
  WHATWG rules) — real-world malformed pages would parse differently than
  a real browser.

Gumbo implements the actual WHATWG HTML5 tree-construction algorithm,
including error recovery (implied `<head>`/`<tbody>` insertion,
auto-closing unclosed `<li>`/`<p>` etc.) — verified directly, see
"Verified" below.

## Build integration — cmake/gumbo.cmake, not vendors/gumbo/CMakeLists.txt
Upstream ships **Meson only**, no CMake. The `gumbo` target is defined in
**`cmake/gumbo.cmake`** (this project's own tree, `include()`d from the
top-level `CMakeLists.txt`) — **not** as a `CMakeLists.txt` inside
`vendors/gumbo/` itself. This matters: a file written directly inside a
git submodule's checkout is invisible to the parent repo's git history —
only `.gitmodules` + the submodule's pinned commit SHA get tracked — so
anything placed inside `vendors/gumbo/` directly would silently vanish on
a fresh clone or on another machine. (This was tried first and caught —
see git history / prior session — then corrected to the current
`cmake/gumbo.cmake` + `include()` approach specifically to fix that.)

It works because gumbo's actual library is small and self-contained: 12
plain C99 `.c` files (`vendors/gumbo/src/*.c`), zero third-party deps, and
every generated artifact (`tag_enum.h`, `tag_gperf.h`, `char_ref_gperf.c`
— normally produced by gperf/python codegen scripts) is already checked
into the repo. So there is no codegen step to replicate — compiling the
`.c` files directly under our own `add_library(gumbo STATIC ...)`,
pointed at `${CMAKE_SOURCE_DIR}/vendors/gumbo/src/*.c` from outside the
submodule, is both correct and simpler than wrapping Meson via
`ExternalProject`/`FetchContent` subprocess tricks. Same underlying idea
as how causality's own `CMakeLists.txt` vendors freetype/glfw/vma without
adopting their upstream build systems wholesale — just relocated one
level up since gumbo's target definition can't safely live inside the
submodule the way causality's vendor `add_subdirectory` calls can (those
point at *causality's* submodules from *causality's own* tracked
CMakeLists.txt, which is a different, safe case — the file doing the
pointing is tracked there).

No `GUMBO_EXPORT`/visibility macros exist in gumbo's public header, so
nothing extra was needed beyond `-fvisibility=hidden` (matching
causality's own convention) on non-Windows.

## Type model
- **`Eng_HtmlDocument`** — owns one parse (`GumboOutput*` underneath).
  Not engine-owned/singleton — `Eng_Engine` has no `.html` field. HTML
  documents are per-navigation (one per page load, eventually one per
  tab), so whatever ends up owning tab/navigation state creates and
  destroys these directly. `eng_html_parse` / `eng_html_document_destroy`
  are called straight from call sites (currently just the `main.c` smoke
  test) rather than threaded through the engine facade.
- **`Eng_HtmlNode`** — implemented as a bare reinterpret-cast of
  `GumboNode*`, **not** a separately heap-allocated wrapper struct.
  Deliberate: gumbo's tree already has stable node pointers for the
  document's lifetime, so wrapping every node would only add allocation
  cost with no safety benefit. The opaque typedef in the header still
  keeps `GumboNode` itself out of every call site outside
  `html_parser.c`. Valid only while the owning `Eng_HtmlDocument` is
  alive — destroying the document invalidates every node handle obtained
  from it, same contract as gumbo's own `gumbo_destroy_output`.

## Parse contract
`eng_html_parse` returns `NULL` **only on allocation failure** — malformed
markup is never a failure case; it's recovered from exactly the way a
real browser's parser would (implied tags inserted, unclosed tags
auto-closed, stray text handled), per the WHATWG spec's error-recovery
rules gumbo implements. Callers should not treat parse failure as "bad
HTML" — that case doesn't exist at this layer; it's for out-of-memory
only.

Unrecognized/custom tags (`GUMBO_TAG_UNKNOWN`, e.g. `<my-widget>`) fall
back to `original_tag` text in `eng_html_node_tag_name` — documented in
the header as an imprecise fallback (includes the source's exact
casing/whitespace, not a clean normalized name); real custom-element
handling should special-case this rather than trust it as a tag string.

## Verified (2026-09-21)
Ran a live parse + recursive tree walk through the full app (see
`main.c`'s HTML smoke test / `log_html_tree`) on:
```html
<html><body>
  <h1 id="title">Hello</h1>
  <p class="intro">World <b>bold</b></p>
  <ul><li>One<li>Two</ul>
</body></html>
```
Confirmed via logged output:
- `<head>` synthesized even though absent from the source — real
  error-recovery, not pass-through.
- `<li>One<li>Two` (deliberately unclosed) parsed into two correctly
  separated, properly nested `<li>` elements — the actual HTML5
  auto-closing algorithm running, not naive/XML-style nesting.
- Attributes (`id="title"`, `class="intro"`), nested inline elements
  (`<b>` inside `<p>`), and mixed text/element children all round-tripped
  correctly through `eng_html_node_child`/`eng_html_node_attribute*`.

## What's NOT covered — the missing piece
**There is no DOM→causality bridge.** Gumbo parses HTML into a tree;
causality separately has a real CSS parser (`ca_css_parse`) and a
layout/paint engine driven by C builder calls (`ca_div_begin`, `ca_text`,
etc.) — but nothing yet walks a parsed `Eng_HtmlDocument`, resolves CSS
against each element (tag/class/id/pseudo-selector matching — causality's
`Ca_Stylesheet` machinery already supports this, it just isn't fed
gumbo's tree), and emits the corresponding `ca_div_begin`/`ca_text`/etc.
calls. That's a genuinely separate, larger design problem (needs a
render-tree/box-tree representation, incremental re-layout strategy,
element↔causality-node identity for updates) — intentionally not
attempted as part of wiring up the parser itself. Next real milestone for
"see a webpage rendered" is this bridge, likely living in
`src/browser/` once that directory gets real content.
