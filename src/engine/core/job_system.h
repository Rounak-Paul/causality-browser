// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Job system instance: a fixed pool of worker threads draining a shared
    queue. One per engine. */
typedef struct Eng_JobSystem Eng_JobSystem;

/** Atomic completion counter for a batch of jobs. Zero-initialize (or use
    eng_job_counter_init) before submitting jobs against it, then block on
    it with eng_job_wait_counter to join. Safe to reuse once it reaches
    zero. Must outlive every job submitted against it. */
typedef struct Eng_JobCounter Eng_JobCounter;

/**
 * Job entry point.
 *
 * user_data  Context pointer passed to eng_job_submit.
 */
typedef void (*Eng_JobFn)(void *user_data);

/**
 * Creates the job system and starts its worker threads.
 *
 * worker_count  Number of worker threads to spawn. 0 selects the engine
 *               default (hardware concurrency minus one, floored at 1,
 *               reserving one logical core for the calling/UI thread).
 * Returns       Heap-allocated job system, or NULL on failure.
 */
Eng_JobSystem *eng_job_system_create(int worker_count);

/**
 * Stops all workers and destroys the job system. Blocks until every
 * in-flight job finishes; any job still queued but not yet started is
 * dropped without running. Do not call from inside a job running on this
 * system (deadlocks).
 */
void eng_job_system_destroy(Eng_JobSystem *js);

/** Returns the number of active worker threads. */
int eng_job_system_worker_count(const Eng_JobSystem *js);

/** Allocates and zero-initializes a counter usable with eng_job_submit /
    eng_job_wait_counter. Free with eng_job_counter_destroy once every
    job referencing it has completed. */
Eng_JobCounter *eng_job_counter_create(void);

/** Releases a counter's storage. The counter must have reached zero
    (no in-flight jobs reference it) before calling this. */
void eng_job_counter_destroy(Eng_JobCounter *counter);

/**
 * Enqueues a job for background execution.
 *
 * js         Target job system.
 * fn         Job entry point; never NULL.
 * user_data  Passed to fn when it runs.
 * counter    Optional; incremented now and decremented when fn returns.
 *            Pass NULL for fire-and-forget jobs with no join point.
 */
void eng_job_submit(Eng_JobSystem *js, Eng_JobFn fn, void *user_data,
                     Eng_JobCounter *counter);

/**
 * Blocks the calling thread until counter reaches zero (every job
 * submitted against it has completed). Returns immediately if the
 * counter is already zero.
 *
 * counter  Counter to wait on.
 */
void eng_job_wait_counter(Eng_JobCounter *counter);

/** Returns true if the counter is currently zero (no in-flight jobs). */
bool eng_job_counter_is_done(const Eng_JobCounter *counter);

#ifdef __cplusplus
}
#endif
