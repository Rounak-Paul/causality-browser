// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#include "job_system.h"
#include "logger.h"

#include <causality.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <unistd.h>
#endif

#define JOB_QUEUE_CAPACITY 4096

struct Eng_JobCounter {
    Ca_Mutex   *mutex;
    Ca_CondVar *condvar;
    int         pending; /* jobs submitted against this counter, not yet run */
};

typedef struct Job {
    Eng_JobFn        fn;
    void            *user_data;
    Eng_JobCounter  *counter;
} Job;

struct Eng_JobSystem {
    Ca_Mutex   *queue_mutex;
    Ca_CondVar *queue_condvar;

    Job    queue[JOB_QUEUE_CAPACITY];
    size_t head;   /* next slot to dequeue */
    size_t count;  /* jobs currently queued */

    Ca_Thread **workers;
    int          worker_count;

    /* Guarded by queue_mutex; workers observe this to exit their loop. */
    bool shutting_down;
};

static int detect_hardware_concurrency(void)
{
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (int)info.dwNumberOfProcessors;
#elif defined(__APPLE__) || defined(__linux__)
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0) ? (int)n : 1;
#else
    return 1;
#endif
}

static void counter_add(Eng_JobCounter *counter, int delta)
{
    if (!counter) return;
    ca_mutex_lock(counter->mutex);
    counter->pending += delta;
    if (counter->pending == 0) ca_condvar_broadcast(counter->condvar);
    ca_mutex_unlock(counter->mutex);
}

static void *worker_main(void *user_data)
{
    Eng_JobSystem *js = (Eng_JobSystem *)user_data;

    for (;;) {
        ca_mutex_lock(js->queue_mutex);
        while (js->count == 0 && !js->shutting_down)
            ca_condvar_wait(js->queue_condvar, js->queue_mutex);

        if (js->count == 0 && js->shutting_down) {
            ca_mutex_unlock(js->queue_mutex);
            break;
        }

        Job job = js->queue[js->head];
        js->head = (js->head + 1) % JOB_QUEUE_CAPACITY;
        --js->count;
        ca_mutex_unlock(js->queue_mutex);

        job.fn(job.user_data);
        counter_add(job.counter, -1);
    }

    return NULL;
}

Eng_JobSystem *eng_job_system_create(int worker_count)
{
    if (worker_count <= 0) {
        int hw = detect_hardware_concurrency();
        worker_count = (hw > 1) ? hw - 1 : 1;
    }

    Eng_JobSystem *js = calloc(1, sizeof(Eng_JobSystem));
    if (!js) return NULL;

    js->queue_mutex   = ca_mutex_create();
    js->queue_condvar = ca_condvar_create();
    js->workers       = calloc((size_t)worker_count, sizeof(Ca_Thread *));
    if (!js->queue_mutex || !js->queue_condvar || !js->workers) {
        if (js->queue_mutex)   ca_mutex_destroy(js->queue_mutex);
        if (js->queue_condvar) ca_condvar_destroy(js->queue_condvar);
        free(js->workers);
        free(js);
        return NULL;
    }

    js->worker_count = worker_count;
    for (int i = 0; i < worker_count; ++i)
        js->workers[i] = ca_thread_create(worker_main, js);

    ENG_LOG_INFO("job", "started %d worker thread(s)", worker_count);
    return js;
}

void eng_job_system_destroy(Eng_JobSystem *js)
{
    if (!js) return;

    ca_mutex_lock(js->queue_mutex);
    js->shutting_down = true;
    ca_mutex_unlock(js->queue_mutex);
    ca_condvar_broadcast(js->queue_condvar);

    for (int i = 0; i < js->worker_count; ++i)
        ca_thread_join(js->workers[i]);

    free(js->workers);
    ca_condvar_destroy(js->queue_condvar);
    ca_mutex_destroy(js->queue_mutex);
    free(js);
}

int eng_job_system_worker_count(const Eng_JobSystem *js)
{
    return js ? js->worker_count : 0;
}

Eng_JobCounter *eng_job_counter_create(void)
{
    Eng_JobCounter *counter = calloc(1, sizeof(Eng_JobCounter));
    if (!counter) return NULL;
    counter->mutex   = ca_mutex_create();
    counter->condvar = ca_condvar_create();
    if (!counter->mutex || !counter->condvar) {
        if (counter->mutex)   ca_mutex_destroy(counter->mutex);
        if (counter->condvar) ca_condvar_destroy(counter->condvar);
        free(counter);
        return NULL;
    }
    return counter;
}

void eng_job_counter_destroy(Eng_JobCounter *counter)
{
    if (!counter) return;
    ca_mutex_destroy(counter->mutex);
    ca_condvar_destroy(counter->condvar);
    free(counter);
}

void eng_job_submit(Eng_JobSystem *js, Eng_JobFn fn, void *user_data,
                     Eng_JobCounter *counter)
{
    if (!js || !fn) return;

    counter_add(counter, 1);

    ca_mutex_lock(js->queue_mutex);
    if (js->count == JOB_QUEUE_CAPACITY) {
        /* Queue exhausted — run inline rather than corrupt state or
           drop the job silently; correctness over throughput here. */
        ca_mutex_unlock(js->queue_mutex);
        ENG_LOG_WARN("job", "queue full (%d), running job inline",
                      JOB_QUEUE_CAPACITY);
        fn(user_data);
        counter_add(counter, -1);
        return;
    }

    size_t tail = (js->head + js->count) % JOB_QUEUE_CAPACITY;
    js->queue[tail] = (Job){ .fn = fn, .user_data = user_data, .counter = counter };
    ++js->count;
    ca_mutex_unlock(js->queue_mutex);

    ca_condvar_signal(js->queue_condvar);
}

void eng_job_wait_counter(Eng_JobCounter *counter)
{
    if (!counter) return;
    ca_mutex_lock(counter->mutex);
    while (counter->pending > 0)
        ca_condvar_wait(counter->condvar, counter->mutex);
    ca_mutex_unlock(counter->mutex);
}

bool eng_job_counter_is_done(const Eng_JobCounter *counter)
{
    return !counter || counter->pending == 0;
}
