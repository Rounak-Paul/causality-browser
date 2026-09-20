// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#pragma once

#include "core/event.h"
#include "core/job_system.h"
#include "core/logger.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Owns the lifetime and startup/shutdown order of engine-wide
    subsystems (logger, event bus, job system). One instance per
    process. */
typedef struct Eng_Engine {
    Eng_EventBus  *events;
    Eng_JobSystem *jobs;
} Eng_Engine;

/**
 * Initializes the logger, event bus, and job system in dependency order.
 *
 * engine        Engine instance to populate; zero-initialize before
 *               calling (a stack or static Eng_Engine works).
 * worker_count  Forwarded to eng_job_system_create; 0 = engine default.
 * Returns       true on success. On failure, any subsystem already
 *               brought up is torn down before returning, leaving
 *               *engine zeroed.
 */
bool eng_engine_init(Eng_Engine *engine, int worker_count);

/**
 * Shuts down every subsystem in reverse init order and zeroes *engine.
 *
 * engine  Engine instance previously initialized with eng_engine_init.
 */
void eng_engine_shutdown(Eng_Engine *engine);

#ifdef __cplusplus
}
#endif
