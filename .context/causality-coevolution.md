# causality co-evolution mandate

`vendors/causality` is **not a frozen third-party dependency** for this
project — it's the user's own library (`github.com/Rounak-Paul/causality`,
`origin` remote, `main` branch), and **Sol** (a separate project) consumes
the exact same causality repo. This changes how work proceeds:

## The mandate, precisely
- **Extend and refactor causality as the browser's real needs surface
  gaps** — don't work around causality's limitations in browser-side
  code when the right fix is in causality itself. The inline-layout gap
  hit while writing [[browser-dom-bridge]] is the first concrete example:
  a hand-rolled horizontal-flex hack in the bridge is a workaround; a real
  inline flow primitive in causality is the actual fix, and belongs there.
- **Not a rewrite.** "Extend and refactor, not fundamentally change" —
  keep causality's existing shape: declarative C builder pattern
  (`ca_*_begin`/`_end`), the real CSS parser + cascade, event-based
  widget callbacks, its CMake/vendor structure. Improve ergonomics and
  fill gaps; don't replace the architecture.
- **Must stay non-breaking for Sol.** Since Sol depends on the same
  causality repo, changes need to be additive (new functions/fields/
  optional params) rather than removing or changing the semantics of
  existing public API (`causality.h`, `ca_components.h`, etc.) that Sol
  may already call. When a genuine breaking change seems necessary, flag
  it explicitly rather than pushing it silently — Sol has no CI/test
  signal visible from here to catch a break automatically.
- **Edit in place, commit inside the submodule.** Work directly in
  `vendors/causality/`, commit there normally (`git commit` inside that
  directory — it's a full clone of the user's own repo), push to
  `origin` when a change is ready, then bump the pin in
  causality-browser's own `git add vendors/causality` (the submodule
  gitlink) so the parent repo tracks the new commit.
- **"Feels like home to frontend devs"** is the design lens for
  ergonomics work: CSS they already know (mostly there), an HTML-shaped
  builder (mostly there), but real DOM/component ergonomics and proper
  inline text flow are gaps worth closing as the browser's own needs
  surface them.

## Practical workflow
1. Identify a real gap by building actual browser functionality against
   causality (not speculatively — let the browser's needs drive it).
2. Make the change in `vendors/causality/` as a focused, additive commit.
3. Verify: causality's own test suite still builds/passes
   (`vendors/causality/causality/tests/`, run via its own `BUILD_TESTING`
   path — note causality-browser's top-level CMakeLists forces
   `BUILD_TESTING=OFF`, so testing causality's own suite requires
   configuring `vendors/causality` standalone or temporarily flipping
   that flag) — plus the browser's own use case works.
4. Push to `origin` (the user's causality repo) once verified.
5. Bump the pin: `git add vendors/causality` in causality-browser,
   commit there too.

## First gap closed: inline layout (2026-09-21)
Causality's layout was flexbox-only (`CA_HORIZONTAL`/`CA_VERTICAL` divs);
no real inline-flow model existed. Added `Ca_DivDesc.inline_flow` — a
real inline formatting context where mixed text + inline elements wrap
together on shared lines, generalizing the existing single-string
word-wrap algorithm to span multiple sibling nodes. Committed and pushed
to `origin`. Full design/implementation notes in
`vendors/causality/.context/inline-formatting-context.md` (lives inside
the submodule's own `.context/`, matching its existing convention — not
duplicated here). [[browser-dom-bridge]] now uses this instead of the
old horizontal-flex-div hack.

First-pass scope, not yet closed: text-only (no click-target
hit-testing inside an inline run — a clickable `<a>` mid-sentence isn't
interactive yet), no nested inline sub-containers, `display: inline` in
CSS isn't auto-derived into `inline_flow` yet (opt-in via the field
only). See the causality-side doc for the full list.
