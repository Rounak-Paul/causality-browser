// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Event bus instance. One per engine; browser code publishes/subscribes
    against the instance returned by eng_engine's init. */
typedef struct Eng_EventBus Eng_EventBus;

/** Opaque subscription handle returned by eng_event_subscribe, used to
    unsubscribe later. */
typedef struct Eng_EventSub Eng_EventSub;

/** Event type identifier. Callers define their own ID space (e.g. an enum
    starting past ENG_EVENT_USER_BASE) — the bus does not interpret it. */
typedef uint32_t Eng_EventId;

/** Reserved below this value for future engine-internal events. */
#define ENG_EVENT_USER_BASE 1000u

/**
 * Payload delivered to a handler. data/data_size describe an
 * event-specific struct owned by the publisher; valid only for the
 * duration of the handler call — copy anything needed past that.
 */
typedef struct Eng_Event {
    Eng_EventId  id;
    const void  *data;
    size_t       data_size;
} Eng_Event;

/**
 * Handler callback invoked synchronously on the publishing thread.
 *
 * event      The published event (valid only for the duration of the call).
 * user_data  Context pointer passed to eng_event_subscribe.
 */
typedef void (*Eng_EventFn)(const Eng_Event *event, void *user_data);

/**
 * Creates a new event bus.
 *
 * Returns  Heap-allocated bus, or NULL on allocation failure.
 */
Eng_EventBus *eng_event_bus_create(void);

/** Destroys the bus and releases all subscription storage. Outstanding
    Eng_EventSub handles into it become invalid. */
void eng_event_bus_destroy(Eng_EventBus *bus);

/**
 * Subscribes a handler to one event id.
 *
 * bus        Target bus.
 * id         Event id to listen for.
 * fn         Handler function; never NULL.
 * user_data  Passed to fn on every dispatch.
 * Returns    Subscription handle for eng_event_unsubscribe, or NULL on
 *            allocation failure.
 */
Eng_EventSub *eng_event_subscribe(Eng_EventBus *bus, Eng_EventId id,
                                   Eng_EventFn fn, void *user_data);

/**
 * Removes a subscription. Safe to call from within a handler (including
 * the handler being removed) — removal is deferred until the current
 * dispatch finishes.
 *
 * bus  Bus the subscription belongs to.
 * sub  Subscription to remove; becomes invalid after this call.
 */
void eng_event_unsubscribe(Eng_EventBus *bus, Eng_EventSub *sub);

/**
 * Publishes an event, synchronously invoking every current subscriber for
 * its id in subscription order. Thread-safe: publishing and
 * subscribing/unsubscribing may happen concurrently from different
 * threads, but handlers for a single publish always run on the calling
 * thread.
 *
 * bus        Target bus.
 * id         Event id to dispatch.
 * data       Optional event-specific payload; NULL if none.
 * data_size  Size of *data in bytes; 0 if data is NULL.
 */
void eng_event_publish(Eng_EventBus *bus, Eng_EventId id,
                        const void *data, size_t data_size);

#ifdef __cplusplus
}
#endif
