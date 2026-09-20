// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#include "event.h"

#include <causality.h>
#include <stdlib.h>
#include <string.h>

/** One registered subscription; a flat array entry, not individually
    heap-allocated, so eng_event_subscribe returns the entry's address as
    the opaque handle — stable as long as the bus is alive (the array
    only compacts removed slots, never relocates live ones on the fly
    within a single publish, and reallocation is guarded by the mutex so
    no publish observes a torn array). */
struct Eng_EventSub {
    Eng_EventId  id;
    Eng_EventFn  fn;
    void        *user_data;
    bool         pending_removal;
};

struct Eng_EventBus {
    Ca_Mutex     *mutex;
    Eng_EventSub *subs;
    size_t        count;
    size_t        capacity;
    /* >0 while a publish is iterating subs; unsubscribe defers physical
       removal until this drops to 0 so in-progress iteration never sees
       a shifted array. */
    int           dispatch_depth;
};

Eng_EventBus *eng_event_bus_create(void)
{
    Eng_EventBus *bus = calloc(1, sizeof(Eng_EventBus));
    if (!bus) return NULL;
    bus->mutex = ca_mutex_create();
    if (!bus->mutex) {
        free(bus);
        return NULL;
    }
    return bus;
}

void eng_event_bus_destroy(Eng_EventBus *bus)
{
    if (!bus) return;
    ca_mutex_destroy(bus->mutex);
    free(bus->subs);
    free(bus);
}

static void compact_removed_locked(Eng_EventBus *bus)
{
    if (bus->dispatch_depth > 0) return;
    size_t write = 0;
    for (size_t read = 0; read < bus->count; ++read) {
        if (bus->subs[read].pending_removal) continue;
        if (write != read) bus->subs[write] = bus->subs[read];
        ++write;
    }
    bus->count = write;
}

Eng_EventSub *eng_event_subscribe(Eng_EventBus *bus, Eng_EventId id,
                                   Eng_EventFn fn, void *user_data)
{
    if (!bus || !fn) return NULL;

    ca_mutex_lock(bus->mutex);

    if (bus->count == bus->capacity) {
        size_t new_cap = bus->capacity ? bus->capacity * 2 : 8;
        Eng_EventSub *grown = realloc(bus->subs, new_cap * sizeof(Eng_EventSub));
        if (!grown) {
            ca_mutex_unlock(bus->mutex);
            return NULL;
        }
        bus->subs     = grown;
        bus->capacity = new_cap;
    }

    Eng_EventSub *sub = &bus->subs[bus->count++];
    sub->id              = id;
    sub->fn               = fn;
    sub->user_data        = user_data;
    sub->pending_removal  = false;

    ca_mutex_unlock(bus->mutex);
    return sub;
}

void eng_event_unsubscribe(Eng_EventBus *bus, Eng_EventSub *sub)
{
    if (!bus || !sub) return;

    ca_mutex_lock(bus->mutex);
    sub->pending_removal = true;
    compact_removed_locked(bus);
    ca_mutex_unlock(bus->mutex);
}

void eng_event_publish(Eng_EventBus *bus, Eng_EventId id,
                        const void *data, size_t data_size)
{
    if (!bus) return;

    Eng_Event event = { .id = id, .data = data, .data_size = data_size };

    ca_mutex_lock(bus->mutex);
    ++bus->dispatch_depth;

    /* Snapshot the current count: subscriptions added mid-dispatch (from
       within a handler) are not visited this publish, matching the
       "subscription order at publish time" contract without needing to
       copy the array. Removed-but-still-present entries are skipped by
       the pending_removal check instead of being physically absent. */
    size_t snapshot_count = bus->count;
    for (size_t i = 0; i < snapshot_count; ++i) {
        Eng_EventSub *sub = &bus->subs[i];
        if (sub->pending_removal || sub->id != id) continue;

        ca_mutex_unlock(bus->mutex);
        sub->fn(&event, sub->user_data);
        ca_mutex_lock(bus->mutex);
    }

    --bus->dispatch_depth;
    compact_removed_locked(bus);
    ca_mutex_unlock(bus->mutex);
}
