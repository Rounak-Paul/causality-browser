// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../core/event.h"
#include "../core/job_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Owns libcurl's process-wide global state (curl_global_init/cleanup —
    must be called exactly once per process, never per-thread/per-fetch;
    see https://curl.se/libcurl/c/curl_global_init.html). One per engine,
    created before any eng_net_fetch call. */
typedef struct Eng_NetSystem Eng_NetSystem;

/** Event published (via the Eng_EventBus passed to eng_net_fetch, if
    non-NULL) whenever a fetch completes, successfully or not. Payload is
    an Eng_NetResult* (see below), valid only for the duration of the
    publish call — subscribers needing the body past that must copy it. */
#define ENG_NET_EVENT_FETCH_COMPLETE (ENG_EVENT_USER_BASE + 1u)

/**
 * Outcome of one eng_net_fetch call, passed to both the completion
 * callback and the ENG_NET_EVENT_FETCH_COMPLETE event payload.
 *
 * On success (ok == true): status is the HTTP response status code,
 * body/body_len is the response body (not null-terminated by
 * assumption — body_len is authoritative; a null terminator IS present
 * at body[body_len] as a convenience for text content, but binary
 * bodies may legitimately contain embedded NUL bytes before that
 * point), content_type is the response's Content-Type header value (or
 * NULL if the server didn't send one).
 *
 * On failure (ok == false): status is 0, body/body_len/content_type are
 * NULL/0/NULL, error holds a human-readable libcurl error string valid
 * for the duration of the callback/event (owned by the fetch job, freed
 * immediately after — copy it if needed past that).
 */
typedef struct Eng_NetResult {
    bool        ok;
    int         status;
    const char *body;
    size_t      body_len;
    const char *content_type;
    const char *error;
    const char *url;   /* the URL that was requested, for correlating a
                           result back to its request when several
                           fetches are in flight concurrently */
} Eng_NetResult;

/**
 * Callback invoked when a fetch completes.
 *
 * IMPORTANT: called on whichever job-system worker thread ran the
 * fetch, never the thread that called eng_net_fetch (unless that
 * happens to be the same worker — not guaranteed). Callbacks that touch
 * causality UI state or anything else that is UI-thread-only must hop
 * back to the UI thread themselves (e.g. by setting a flag an
 * on_frame callback checks, or publishing a signal the UI thread
 * polls) — this mirrors the exact same constraint Eng_EventBus's
 * publish-on-calling-thread contract already documents.
 *
 * result     Valid only for the duration of this call — its body/error
 *            strings are freed immediately after the callback returns.
 * user_data  Context pointer passed to eng_net_fetch.
 */
typedef void (*Eng_NetFetchFn)(const Eng_NetResult *result, void *user_data);

/**
 * Creates the network subsystem, initializing libcurl's global state.
 * Must be called exactly once per process before any eng_net_fetch call
 * (matches curl_global_init's own one-call-per-process contract).
 *
 * Returns  Heap-allocated net system, or NULL on failure.
 */
Eng_NetSystem *eng_net_system_create(void);

/** Destroys the net system and releases libcurl's global state. Every
    eng_net_fetch job submitted against this system's job system must
    have completed first (wait on your own counter — eng_net_fetch takes
    an optional Eng_JobCounter for exactly this). */
void eng_net_system_destroy(Eng_NetSystem *net);

/**
 * Fetches a URL via HTTP(S) GET, running the request on a job-system
 * worker thread — never blocks the calling thread.
 *
 * net         Owning net system (must outlive the fetch).
 * jobs        Job system to run the request on.
 * url         Null-terminated URL to fetch (http:// or https://).
 * on_complete Called with the result once the fetch finishes; NULL to
 *             skip (e.g. when only the event-bus path is wanted).
 * user_data   Passed to on_complete.
 * events      Optional; if non-NULL, ENG_NET_EVENT_FETCH_COMPLETE is
 *             published on it after on_complete returns, from the same
 *             worker thread. NULL to skip.
 * counter     Optional; incremented/decremented exactly like any other
 *             eng_job_submit call, so a caller can eng_job_wait_counter
 *             to block until the fetch (and its callback) has run.
 */
void eng_net_fetch(Eng_NetSystem *net, Eng_JobSystem *jobs,
                    const char *url,
                    Eng_NetFetchFn on_complete, void *user_data,
                    Eng_EventBus *events, Eng_JobCounter *counter);

#ifdef __cplusplus
}
#endif
