// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#include "engine.h"

#include <string.h>

bool eng_engine_init(Eng_Engine *engine, int worker_count)
{
    memset(engine, 0, sizeof(*engine));

    eng_log_init();
    ENG_LOG_INFO("engine", "initializing");

    engine->events = eng_event_bus_create();
    if (!engine->events) {
        ENG_LOG_FATAL("engine", "failed to create event bus");
        eng_log_shutdown();
        return false;
    }

    engine->jobs = eng_job_system_create(worker_count);
    if (!engine->jobs) {
        ENG_LOG_FATAL("engine", "failed to create job system");
        eng_event_bus_destroy(engine->events);
        engine->events = NULL;
        eng_log_shutdown();
        return false;
    }

    /* 0 = no memory cap for now; revisit once per-tab budgets matter. */
    engine->js = eng_js_runtime_create(0);
    if (!engine->js) {
        ENG_LOG_FATAL("engine", "failed to create JS runtime");
        eng_job_system_destroy(engine->jobs);
        engine->jobs = NULL;
        eng_event_bus_destroy(engine->events);
        engine->events = NULL;
        eng_log_shutdown();
        return false;
    }

    engine->net = eng_net_system_create();
    if (!engine->net) {
        ENG_LOG_FATAL("engine", "failed to create net system");
        eng_js_runtime_destroy(engine->js);
        engine->js = NULL;
        eng_job_system_destroy(engine->jobs);
        engine->jobs = NULL;
        eng_event_bus_destroy(engine->events);
        engine->events = NULL;
        eng_log_shutdown();
        return false;
    }

    ENG_LOG_INFO("engine", "ready (%d worker thread(s))",
                 eng_job_system_worker_count(engine->jobs));
    return true;
}

void eng_engine_shutdown(Eng_Engine *engine)
{
    if (!engine) return;

    ENG_LOG_INFO("engine", "shutting down");

    if (engine->net)    eng_net_system_destroy(engine->net);
    if (engine->js)     eng_js_runtime_destroy(engine->js);
    if (engine->jobs)   eng_job_system_destroy(engine->jobs);
    if (engine->events) eng_event_bus_destroy(engine->events);

    eng_log_shutdown();

    memset(engine, 0, sizeof(*engine));
}
