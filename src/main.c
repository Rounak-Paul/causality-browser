// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#include <causality.h>

#include "engine/engine.h"

/* App-level event ids, defined past the engine's reserved range. */
enum {
    EVT_ENGINE_READY = ENG_EVENT_USER_BASE,
};

static void on_engine_ready(const Eng_Event *event, void *user_data)
{
    (void)user_data;
    ENG_LOG_INFO("app", "received EVT_ENGINE_READY (id=%u)", event->id);
}

static void startup_probe_job(void *user_data)
{
    Eng_Engine *engine = (Eng_Engine *)user_data;
    ENG_LOG_INFO("app", "startup probe job ran on a worker thread");
    eng_event_publish(engine->events, EVT_ENGINE_READY, NULL, 0);
}

/**
 * Application entry point.
 *
 * Brings up the engine (logger, event bus, job system), opens the main
 * Causality window, then runs the render/event loop until the window is
 * closed.
 *
 * Returns 0 on clean shutdown, 1 if engine, instance, or window creation
 * fails.
 */
int main(void)
{
    Eng_Engine engine;
    if (!eng_engine_init(&engine, 0)) {
        return 1;
    }

    Ca_InstanceDesc instance_desc = {
        .app_name             = "Causality Browser",
        .prefer_dedicated_gpu = true,
    };
    Ca_Instance *instance = ca_instance_create(&instance_desc);
    if (!instance) {
        ENG_LOG_FATAL("app", "failed to create causality instance");
        eng_engine_shutdown(&engine);
        return 1;
    }

    Ca_WindowDesc window_desc = {
        .title  = "Causality Browser",
        .width  = 1280,
        .height = 800,
    };
    Ca_Window *window = ca_window_create(instance, &window_desc);
    if (!window) {
        ENG_LOG_FATAL("app", "failed to create window");
        ca_instance_destroy(instance);
        eng_engine_shutdown(&engine);
        return 1;
    }

    /* Startup smoke test: subscribe, run a job on the pool that publishes
       back to the UI thread's bus, then join it before entering the
       render loop. Exercises logger + event bus + job system together. */
    Eng_EventSub *sub = eng_event_subscribe(engine.events, EVT_ENGINE_READY,
                                             on_engine_ready, NULL);
    Eng_JobCounter *startup_counter = eng_job_counter_create();
    eng_job_submit(engine.jobs, startup_probe_job, &engine, startup_counter);
    eng_job_wait_counter(startup_counter);
    eng_job_counter_destroy(startup_counter);
    eng_event_unsubscribe(engine.events, sub);

    while (ca_instance_tick(instance)) { }

    ca_instance_destroy(instance);
    eng_engine_shutdown(&engine);
    return 0;
}
