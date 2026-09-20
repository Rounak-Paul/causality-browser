// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** One JS heap/GC arena. One per engine — every Eng_JsContext (tab/frame)
    created from it shares its garbage collector and memory limit, but
    each gets its own global object and module registry. Wraps quickjs-ng's
    JSRuntime; the raw type never crosses this header. */
typedef struct Eng_JsRuntime Eng_JsRuntime;

/** One JS global scope. A browser creates one per tab/frame/worker so
    pages cannot see each other's globals; all contexts from the same
    Eng_JsRuntime share its GC and memory budget. Wraps quickjs-ng's
    JSContext. */
typedef struct Eng_JsContext Eng_JsContext;

/**
 * Creates a JS runtime (heap + GC arena).
 *
 * memory_limit_bytes  Hard cap on total JS heap size; 0 = no limit
 *                      (quickjs-ng default).
 * Returns             Heap-allocated runtime, or NULL on failure.
 */
Eng_JsRuntime *eng_js_runtime_create(size_t memory_limit_bytes);

/** Destroys a runtime. Every Eng_JsContext created from it must already
    be destroyed — destroying a runtime with live contexts is undefined
    behavior in the underlying engine, so callers must track and free
    their contexts first. */
void eng_js_runtime_destroy(Eng_JsRuntime *runtime);

/**
 * Runs one pass of pending microtasks (resolved promise reactions,
 * queued module evaluations). Call once per frame/tick from the owning
 * thread — quickjs-ng never runs jobs on its own.
 *
 * runtime  Runtime whose job queue to drain.
 * Returns  Number of jobs executed this call (0 if the queue was empty).
 */
int eng_js_runtime_run_jobs(Eng_JsRuntime *runtime);

/** Forces an immediate full garbage-collection pass. Normally unnecessary
    (quickjs-ng collects incrementally), useful for memory-pressure
    handling or leak diagnostics. */
void eng_js_runtime_collect_garbage(Eng_JsRuntime *runtime);

/**
 * Creates a new global scope (context) on a runtime.
 *
 * runtime  Owning runtime; must outlive the returned context.
 * Returns  Heap-allocated context, or NULL on failure.
 */
Eng_JsContext *eng_js_context_create(Eng_JsRuntime *runtime);

/** Destroys a context. Must be called before its owning runtime is
    destroyed. */
void eng_js_context_destroy(Eng_JsContext *context);

/** Outcome of an eng_js_eval call. On success, value is the string
    representation of the completion value (e.g. "42", "undefined") and
    error is empty. On failure, value is empty and error holds the
    exception's message (or a generic description if the exception itself
    could not be stringified). Both strings are owned by this struct;
    free with eng_js_result_free once done. */
typedef struct Eng_JsResult {
    bool  ok;
    char *value;
    char *error;
} Eng_JsResult;

/**
 * Evaluates a script as global code and returns its completion value.
 *
 * context     Context to evaluate in.
 * source      JS source text; need not be null-terminated at source_len.
 * source_len  Length of source in bytes.
 * filename    Name reported in stack traces/errors (e.g. the page URL).
 * Returns     Result struct; always call eng_js_result_free on it.
 */
Eng_JsResult eng_js_eval(Eng_JsContext *context, const char *source,
                          size_t source_len, const char *filename);

/** Frees the strings owned by a result previously returned by
    eng_js_eval. Safe to call on a zero-initialized or already-freed
    result. */
void eng_js_result_free(Eng_JsResult *result);

#ifdef __cplusplus
}
#endif
