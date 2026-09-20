# engine/js — quickjs-ng wrapper

`src/engine/js/js_runtime.{h,c}` wraps quickjs-ng (`vendors/quickjs`,
pinned `v0.17.0`, MIT). See `.context/project-setup.md` for overall
layout. Same discipline as `engine/core`: raw vendor types (`JSRuntime*`,
`JSContext*`, `JSValue`) never cross the header — callers only see
`Eng_JsRuntime`, `Eng_JsContext`, `Eng_JsResult`.

## Why quickjs-ng over alternatives
Pure C (matches this project's C11 standard, no C++ embedding
friction like V8/JSC), MIT license, small (~1MB), spec-complete
(ES2023-class: classes, async/await, modules, BigInt, Proxy), and a
proper CMake `add_library(qjs ...)` target — no Bazel/GN build system to
fight, unlike V8. No JIT (bytecode-interpreted), but fastest
non-JIT engine available; JIT-class perf is a later swap-the-engine
decision, not a bootstrap-time one.

## Type model
- **`Eng_JsRuntime`** — one JS heap/GC arena. **One per engine/process**
  (owned by `Eng_Engine.js`, created in `eng_engine_init`). Every context
  created from it shares its GC and memory budget.
- **`Eng_JsContext`** — one JS global scope. Intended to be **one per
  tab/frame/worker** once the browser has tabs — pages must not share a
  global object. All contexts from one runtime still share that runtime's
  GC, so a leak/heavy-alloc page can pressure other tabs' collection
  cadence; per-tab memory budgets would need per-context accounting layered
  on top later if that becomes a problem (`JS_SetMemoryLimit` is
  runtime-wide, not context-wide, in quickjs-ng).

## Eval / result contract
`eng_js_eval(context, source, source_len, filename)` returns `Eng_JsResult`
by value: `{ok, value, error}`, both strings heap-owned by the result —
**always call `eng_js_result_free`**, success or failure, to avoid leaking.
`value` is the completion value stringified via `JS_ToCString` (so `"7"`,
`"undefined"`, `"[object Object]"`, etc. — not a structured value; there is
no typed-value accessor yet, add one when a caller needs to read back
something other than a display string, e.g. a `bool`/`double`/`JSON`
round-trip for a native↔JS bridge).

`filename` is reported in stack traces / error messages — pass the page's
URL once navigation exists; `"<script>"` default otherwise.

## Microtask draining
quickjs-ng **never runs its own job queue** — Promise `.then` reactions,
`async`/`await` continuations, and queued module evaluations only execute
when something calls `JS_ExecutePendingJob`. `eng_js_runtime_run_jobs`
wraps that in a drain loop (runs until the queue is empty or a job
throws) and **must be called every frame/tick** from the owning thread —
`main.c`'s render loop calls it once per `ca_instance_tick` iteration. A
job that throws is logged (`ENG_LOG_ERROR`, tag `"js"`) and draining
continues — one bad unhandled-rejection-style throw must not stall every
other queued reaction.

Verified with a standalone probe (not kept in the tree): `eng_js_eval`
scheduling `Promise.resolve(42).then(...)` returns immediately with the
reaction still pending; one `eng_js_runtime_run_jobs` call drains exactly
that one job and the mutated global becomes visible afterward. Confirms
the eval-returns-before-microtasks-drain contract holds as designed.

## Lifecycle ordering
Contexts must be destroyed before their owning runtime — `eng_engine.c`
currently only creates/destroys the one process-wide runtime and no
persistent context (the startup probe in `main.c` creates and destroys
its own short-lived context inline). Once tabs exist, whatever owns tab
lifecycle must destroy each tab's `Eng_JsContext` before
`eng_engine_shutdown` runs `eng_js_runtime_destroy` — the header docs this
as caller-tracked, not enforced by the wrapper (quickjs-ng's own behavior
on a runtime-with-live-contexts free is undefined, so this is a real
constraint, not just documentation hygiene).

## Build integration notes
- `vendors/quickjs` added with `EXCLUDE_FROM_ALL` in the top-level
  `CMakeLists.txt` — its `CMakeLists.txt` unconditionally defines several
  executables not gated by `BUILD_TESTING` (`qjs_exe`, `qjsc`, `api-test`,
  `lre-test`, `run-test262`, `unicode_gen`); `EXCLUDE_FROM_ALL` keeps them
  out of the default `cmake --build` without patching quickjs's own
  CMakeLists. The `qjs` static library we link still builds normally as a
  real dependency edge.
- `QJS_ENABLE_INSTALL` forced `OFF` — its default `ON` adds `install()`
  rules for a standalone quickjs package we don't want.
- `BUILD_SHARED_LIBS` already defaults `OFF` upstream — matches this
  project's static-link convention, no override needed.
