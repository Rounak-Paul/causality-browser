# engine/core — logger, event bus, job system

Three foundational, browser-domain-agnostic subsystems under `src/engine/`.
See `.context/project-setup.md` for overall layout. All three build on causality's
`Ca_Mutex` / `Ca_CondVar` / `Ca_Thread` (causality.h) rather than raw
pthreads, to keep one threading primitive source in the whole codebase.

## Logger (`core/logger.{h,c}`)
- `ENG_LOG_TRACE/DEBUG/INFO/WARN/ERROR/FATAL(tag, fmt, ...)` macros —
  capture `__FILE__`/`__LINE__` automatically, call `eng_log_write`.
- Level-filtered at the call site (cheap: level compare before formatting).
  Default level `ENG_LOG_INFO`; `eng_log_set_level` to change.
- Thread-safe via one `Ca_Mutex`; `eng_log_init`/`eng_log_shutdown` own it.
- Optional `Eng_LogSinkFn` sink (`eng_log_set_sink`) for a future in-app
  console/devtools panel — called under the log mutex alongside the
  built-in stdout(info/debug/trace)/stderr(warn+) output.
- **Buffering gotcha**: stdout is fully buffered off a TTY. Redirecting
  to a file from a script shows nothing until flush/exit — not a bug, see
  `.context/project-setup.md`'s testing note.

## Event bus (`core/event.{h,c}`)
- `Eng_EventBus` — one per `Eng_Engine`. Synchronous, in-process pub/sub
  keyed by `Eng_EventId` (a plain `uint32_t`; callers define their own
  enum starting at `ENG_EVENT_USER_BASE = 1000` so the engine can add
  reserved internal event ids below that later without collisions).
- `eng_event_publish` dispatches to every current subscriber for that id,
  in subscription order, **on the calling thread** (deliberately not
  queued/async — the job system is the async primitive; keeping publish
  synchronous keeps ordering and lifetime reasoning simple). A job can
  publish from a worker thread; the subscriber's callback then runs on
  that worker thread, not the UI thread — callers that need UI-thread
  delivery must hop themselves (e.g. via a flag `ca_window_set_on_frame`
  checks, or a follow-up job back onto the main loop).
- Subscribe/unsubscribe are safe to call from inside a handler, including
  unsubscribing the handler currently running: removal is deferred
  (`pending_removal` flag) until the active publish's dispatch depth hits
  zero, so no in-progress iteration ever sees a shifted array.
- Implementation: single growable array of subscriptions + one
  `Ca_Mutex`, not per-event-id bucketing. Fine at expected browser-event
  volumes (navigation, tab lifecycle, UI state) — revisit only if profiling
  shows publish-time linear scan actually matters.

## Job system (`core/job_system.{h,c}`)
- `Eng_JobSystem` — fixed pool, size = `hardware_concurrency - 1` by
  default (reserves one logical core for the UI/main thread; pass an
  explicit `worker_count` to override). Verified on the dev M3 Pro:
  `hw.ncpu` is 11 there (not 12), so 10 workers spawn correctly.
- Single shared bounded ring-buffer queue (`JOB_QUEUE_CAPACITY = 4096`)
  guarded by one `Ca_Mutex` + `Ca_CondVar`; **not** lock-free / work-stealing
  by design — a browser engine bootstrap doesn't need that complexity yet,
  and a mutexed queue is trivial to reason about and extend (priorities,
  per-worker queues) later if profiling ever demands it.
- Queue-full fallback: rather than block the submitting thread or drop the
  job, `eng_job_submit` runs it **inline** on the calling thread and logs a
  warning. Correctness-over-throughput choice — never silently loses work.
- Dependency/join model: **counters**, not futures. `Eng_JobCounter` is an
  atomic-via-mutex pending-count; `eng_job_submit(js, fn, data, counter)`
  increments it before enqueueing, each job decrements on completion and
  broadcasts when it hits zero. `eng_job_wait_counter` blocks the calling
  thread until then. One counter can be shared across a whole batch for a
  single join point (fork-join pattern) — this was the explicit design
  choice over per-job futures (simpler in C, no generic result-type
  plumbing needed); see main.c's startup probe job for the minimal
  submit → wait → destroy pattern.
- `eng_job_system_destroy` joins all workers after signalling shutdown;
  any job still queued but not yet dequeued at that point is dropped
  without running (documented in the header) — callers needing guaranteed
  drain should wait on their own counters before calling destroy.

## `Eng_Engine` facade (`engine.h`/`engine.c`)
Owns init/shutdown order: logger → event bus → job system, reverse on
shutdown. `main.c` calls `eng_engine_init(&engine, 0)` once at startup and
`eng_engine_shutdown(&engine)` once at exit; everything else goes through
`engine.events` / `engine.jobs`. On any subsystem failing to init, already-
initialized subsystems are torn down before returning false — no partial-
init state leaks to the caller.
