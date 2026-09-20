# browser/dom_bridge — DOM → causality render bridge

`src/browser/dom_bridge.{h,c}` walks a parsed `Eng_HtmlDocument`
([[engine-html-parser]]) and emits the equivalent causality UI tree via
`ca_div_begin`/`ca_text`/`ca_image`/etc. This is the piece that makes
"parse HTML" and "causality can render styled boxes" actually connect —
see `.context/project-setup.md`'s "Important gap" note, which this file
closes (partially — see "What's still missing" below).

## Core design facts
- **Pure function, no persistent state.** Causality's UI builder is
  immediate-mode: the whole tree is rebuilt from scratch every time
  `ca_ui_begin`/`ca_ui_end` runs, or every time a `ca_div_set_builder`
  callback fires. `eng_dom_render` just re-walks the DOM and re-emits
  every `ca_*` call each time — there is no diffing, no retained bridge
  state, no persistent node mapping between DOM elements and causality
  nodes. This is the correct model for causality's actual architecture,
  not a shortcut.
- **CSS resolution is NOT done by this bridge.** Each emitted element
  passes its HTML `id`/`class` attributes straight through to
  `Ca_DivDesc.id`/`.style` unchanged (both are already the same
  space-separated-class-name shape on both sides, zero translation
  needed). Causality's own `ca_div_begin`/`ca_text`/etc. internally call
  `ca_style_resolve_layers` against whatever `Ca_Stylesheet` is attached
  via `ca_instance_set_stylesheet` — the bridge does no selector matching
  itself. Confirmed by reading `causality/src/ui/widget.c` directly
  (`ca_div_begin` calls `ca_style_resolve_layers` internally).
- **Integration point**: `ca_div_set_builder(content_div,
  eng_dom_render_builder, doc)` — `eng_dom_render_builder` is a thin
  `void(Ca_Div*, void*)` wrapper around `eng_dom_render` matching
  causality's builder-callback signature exactly, so it can be passed
  directly with no cast. `Ca_Div` is forward-declared in
  `dom_bridge.h` (`typedef struct Ca_Node Ca_Div;`, matching
  `causality.h`'s own typedef) rather than including `causality.h`
  there, since only the pointer type is needed at that header's level.

## Tag → render-kind mapping
A small hardcoded dispatch table (`tag_kind_for` in `dom_bridge.c`), not
a generic "everything is a div" fallback — chosen deliberately (see prior
session's AskUserQuestion) because non-rendering tags (`<script>`,
`<style>`, `<head>`, `<meta>`, `<link>`, `<title>`) must be **skipped
entirely**, including their text content — a generic walker would
otherwise try to render a `<script>` tag's JS source as visible page
text, which is wrong. Kinds:
- `TAG_BLOCK` — default; `ca_div_begin`/`end`, `CA_VERTICAL`. Covers
  div/p/section/article/header/footer/nav/ul/li/h1-h6/body/html/form/etc.
- `TAG_INLINE` — span/a/b/strong/i/em/small/label/code;
  `ca_div_begin`/`end`, `CA_HORIZONTAL`. **Known hack, not real inline
  flow** — see "What's still missing" below.
- `TAG_IMAGE` — `<img>`; renders a fixed 32×32 gray placeholder box (+
  `alt` text if present) rather than a real image, since there is no
  decoder or network fetch yet.
- `TAG_BREAK` — `<br>`; a 16px spacer, no children.
- `TAG_SKIP` — head/script/style/title/meta/link/noscript/template;
  contributes nothing to the visual tree (children not walked at all).

Unrecognized tags fall through to `TAG_BLOCK` (the safer default — a
misclassified inline element getting an unwanted line break is a much
smaller problem than a misclassified block element losing its layout).

Whitespace-only text nodes are skipped (`is_blank`) — real HTML is full
of insignificant whitespace between tags that would otherwise emit empty
`ca_text` calls and pollute layout.

`<body>` lookup: `find_first(root, "body")` does a depth-first search
from the document root rather than assuming `root == <html>`'s first
child is `<body>` — gumbo always synthesizes both `<head>` and `<body>`
per HTML5 error-recovery rules regardless of what the source actually
contained (verified — see [[engine-html-parser]]'s "Verified" section),
so this search is robust to that synthesis without hardcoding tree shape
assumptions.

## Verified (2026-09-21)
Ran the full app with a real demo page (headings, styled paragraphs via
a parsed stylesheet, a list, an inline link/bold, an image placeholder)
through `ca_ui_begin` → `ca_div_set_builder` → `eng_dom_render_builder` →
`eng_dom_render`. Confirmed via a temporary instrumentation pass (added,
verified it fires exactly once per render with no WARN/ERROR, then
reverted — not left in the tree) that the render path actually executes
end to end with no crash. Visual/pixel verification was not possible in
this sandboxed session (`screencapture`/accessibility APIs both denied —
same restriction hit earlier for window-title inspection); code-level
review plus the no-crash/no-error confirmation is the verification bar
that was actually achievable here. **A human should visually confirm the
render looks reasonable** the first time this runs on an unrestricted
machine.

## What's still missing before this can point at a live URL
This closes the render half of the pipeline for **static, offline** HTML.
Still needed, roughly in the order a real page load would hit them:
1. **Network fetch** (HTTPS GET, redirects, gzip/br decompression) — not
   started. Likely `libcurl`, per the earlier plan discussion — TLS/
   redirects/compression are all solved problems there, not worth
   hand-rolling.
2. **Linked/inline stylesheet extraction** — `<style>` tag content and
   `<link rel="stylesheet" href>` fetch aren't wired to `ca_css_parse`
   yet; the demo page's CSS is hand-written and attached manually in
   `main.c`, not extracted from the parsed document.
3. **Real inline text flow** — see [[causality-coevolution]]'s "Known
   first gap" section. `TAG_INLINE`'s horizontal-flex-div hack does not
   actually wrap mixed inline+text content across a flowing line the way
   a browser's real inline formatting context does. This is the leading
   candidate for the first real causality extension (not a browser-side
   workaround) — per the new co-evolution mandate, the right fix lives in
   causality's layout engine, not another bridge-side hack.
4. **Real image loading** — `<img>` needs a decoder (PNG/JPEG at
   minimum) feeding `ca_image_create`'s raw-RGBA-buffer API, plus the
   network fetch from (1) to actually retrieve image bytes.
5. **JS DOM bindings** — quickjs-ng ([[engine-js-runtime]]) runs
   scripts but has zero DOM API surface (`document.*`, `window.*`, event
   listeners) exposed into the JS global object yet; `<script>` tags are
   currently just skipped by the bridge (`TAG_SKIP`), never executed.
