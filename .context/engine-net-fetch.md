# engine/net — libcurl-backed HTTP(S) fetch

`src/engine/net/net_fetch.{h,c}` wraps libcurl's "easy" API for
asynchronous (relative to the caller) HTTP(S) GET. See
`.context/project-setup.md` for overall layout.

## Why not vendored
Every other third-party dependency in this project (causality, quickjs,
gumbo) is a git submodule built from source. libcurl is the one
exception: **curl's own CMake build has no bundled/vendored TLS backend
option** — HTTPS requires an external TLS library (OpenSSL, mbedTLS,
wolfSSL, GnuTLS, or Rustls; none bundled). Vendoring curl would mean also
vendoring and indefinitely maintaining a TLS library — a much bigger,
more security-sensitive undertaking than gumbo/quickjs (verified by
reading curl's own `CMakeLists.txt` directly, not assumed). macOS and
Linux both ship a working `libcurl` + TLS backend out of the box, so
`find_package(CURL REQUIRED)` (top-level `CMakeLists.txt`) is used
instead — the same pattern already used for Vulkan. Windows needs curl
installed separately (e.g. via vcpkg) — a normal dev-setup step, not
something this project solves.

## Architecture — why this shape
Runs entirely on top of the two subsystems already built for exactly
this purpose:
- **Job system** (`Eng_JobSystem`): a fetch is one `eng_job_submit` job.
  curl's easy API is synchronous-per-call (`curl_easy_perform` blocks
  until the transfer finishes or fails), so the job *is* the async
  boundary — no async curl multi-handle/event-loop machinery needed.
- **Event bus** (`Eng_EventBus`, optional per-call): `eng_net_fetch`
  takes both a direct completion callback (the normal "I made this
  request, give me the response" path) and an optional event-bus
  publish (`ENG_NET_EVENT_FETCH_COMPLETE`) for decoupled observers (e.g.
  future devtools/logging that didn't initiate the fetch). Both fire
  from the worker thread that ran the job — documented explicitly in
  the header, matching `Eng_EventBus`'s own publish-on-calling-thread
  contract.
- **`Eng_NetSystem`**: owns `curl_global_init`/`curl_global_cleanup`,
  which curl's own docs require be called **exactly once per process**
  (never per-thread, never per-fetch) — wired into `Eng_Engine` as a
  fourth subsystem alongside events/jobs/js, same init/shutdown-order
  pattern.

## Two real bugs caught by actual execution, not review
Both were use-after-scope bugs where a pointer into transient storage
was read after that storage's lifetime ended — the kind of bug that
compiles cleanly and often *appears* to work (stack/freed memory isn't
always immediately overwritten), so they were only caught by actually
running a fetch and inspecting the result field byte-for-byte, not by
reading the code:

1. **`content_type` use-after-free**: `CURLINFO_CONTENT_TYPE`'s returned
   pointer is owned by the curl easy handle and only valid until
   `curl_easy_cleanup`. The first version stored it directly into
   `result.content_type` and called `curl_easy_cleanup` before
   delivering `result` to the callback — the string was already freed
   by delivery time. Caught via a live local-server test: the logged
   Content-Type was garbage bytes instead of `text/html; charset=utf-8`.
   Fixed by `strdup`-style copying the string into caller-owned memory
   *before* `curl_easy_cleanup` runs, freed after delivery.
2. **`error_buf` scope-exit read**: `CURLOPT_ERRORBUFFER` writes into a
   caller-provided buffer (not curl-owned — that part was fine), but the
   buffer was declared inside the `else` block that also calls
   `curl_easy_perform`, while `result.error` (pointing at it) is read
   later, **outside** that block, when delivering to
   `job->on_complete`. Undefined behavior per the C standard (the
   array's storage duration ends at the block's closing brace) even
   though the bytes happened to still be intact when tested — caught by
   reasoning about scope after fixing bug #1 revealed the same class of
   mistake was plausible here too, not by a tool flagging it. Fixed by
   declaring `error_buf` at function scope alongside `result` and
   `content_type_copy`.

Lesson generalized in-code as a comment at both fix sites: *anything
delivered to `job->on_complete`/the event publish must be copied into
storage that outlives the block it was populated in* — `body.data`
(heap-allocated, already correct), `content_type_copy` (now heap,
fixed), `error_buf` (now function-scope stack, fixed) all satisfy this;
the pre-fix versions of #1 and #2 didn't.

## Verified
Sandbox environment blocks external DNS/network egress entirely
(confirmed independently via plain `curl` from the shell — not a
code issue). Verified both paths that are reachable:
- **Failure path** (DNS timeout against `https://example.com/`):
  correct graceful failure after the full 30s `CURLOPT_TIMEOUT_MS`,
  clean accurate error message (`"Resolving timed out after 30002
  milliseconds"`), delivered through the callback, app continues
  without hanging/crashing. Re-verified clean after the `error_buf`
  fix (no garbage, no truncation).
- **Success path** (temporarily pointed at a local `python3 -m
  http.server`-equivalent on `127.0.0.1`, reverted after testing —
  `main.c`'s committed smoke test targets a real external URL):
  `HTTP 200`, exact byte count match on the body, and — after the fix —
  the correct `Content-Type` string.

Not verified in this environment (would need real external network
access): actual HTTPS/TLS handshake against a real server, redirect
following, gzip/br decompression. These are all standard curl behavior
this wrapper doesn't reimplement (curl handles TLS/redirects/
decompression internally once `CURLOPT_FOLLOWLOCATION`/
`CURLOPT_ACCEPT_ENCODING` are set, which they are) — low risk, but
worth a real-network smoke test on an unrestricted machine before
relying on this for actual page loads.

## Next step for a live page load
`eng_net_fetch`'s result `body`/`body_len` feed directly into
`eng_html_parse` ([[engine-html-parser]]) the same way `main.c`'s
current hardcoded HTML string does — that wiring (fetch → parse →
[[browser-dom-bridge]] render) is the next integration point, not yet
done. CSS extraction from the fetched page (inline `<style>`, linked
`<link rel="stylesheet">` — itself another fetch) is also still
unwired, per [[browser-dom-bridge]]'s existing "what's missing" list.
