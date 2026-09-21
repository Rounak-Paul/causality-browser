// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#include "net_fetch.h"
#include "../core/logger.h"

#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

struct Eng_NetSystem {
    int placeholder; /* curl_global_init/cleanup are the only real state,
                         both process-global — this struct exists so the
                         type still has a lifetime callers can reason
                         about and extend later (e.g. a shared
                         CURLSH connection-reuse handle). */
};

/* Growable response-body buffer accumulated across possibly-many
   CURLOPT_WRITEFUNCTION callback invocations (curl delivers the body in
   arbitrarily-sized chunks, not necessarily all at once). */
typedef struct FetchBuffer {
    char  *data;
    size_t len;
    size_t capacity;
} FetchBuffer;

static bool fetch_buffer_append(FetchBuffer *buf, const char *chunk, size_t chunk_len)
{
    if (buf->len + chunk_len + 1 > buf->capacity) {
        size_t new_cap = buf->capacity ? buf->capacity * 2 : 4096;
        while (new_cap < buf->len + chunk_len + 1) new_cap *= 2;
        char *grown = realloc(buf->data, new_cap);
        if (!grown) return false;
        buf->data = grown;
        buf->capacity = new_cap;
    }
    memcpy(buf->data + buf->len, chunk, chunk_len);
    buf->len += chunk_len;
    buf->data[buf->len] = '\0'; /* convenience NUL — see Eng_NetResult docs */
    return true;
}

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    FetchBuffer *buf = (FetchBuffer *)userdata;
    size_t total = size * nmemb;
    if (!fetch_buffer_append(buf, ptr, total)) return 0; /* signals error to curl */
    return total;
}

typedef struct FetchJob {
    Eng_NetSystem  *net;
    char           *url;      /* owned copy — source string may not
                                  outlive the caller's own stack frame */
    Eng_NetFetchFn  on_complete;
    void           *user_data;
    Eng_EventBus   *events;
} FetchJob;

static void run_fetch(void *raw_job)
{
    FetchJob *job = (FetchJob *)raw_job;
    Eng_NetResult result = { .ok = false, .url = job->url };
    FetchBuffer body = {0};
    /* CURLINFO_CONTENT_TYPE's returned pointer is owned by the easy
       handle and only valid until curl_easy_cleanup — must be copied
       into memory this function owns before cleanup runs, not just
       stored by pointer into result.content_type (that version compiled
       fine and only showed garbage bytes at delivery time, after
       cleanup had already freed the string it pointed into). Scoped at
       function level, not inside the success branch below, so the one
       delivery/cleanup block at the bottom can free it unconditionally
       regardless of which path set it. */
    char *content_type_copy = NULL;
    /* CURLOPT_ERRORBUFFER writes into caller-provided storage, so this
       one is NOT curl-owned — but it still must outlive the block it's
       declared in: result.error is read at job->on_complete below,
       which is outside error_buf's enclosing block, so pointing at a
       block-scoped stack array there is undefined behavior even though
       the bytes often happen to still be intact in practice. Declaring
       it here, at the same function-level scope result/content_type_copy
       already use, is the actual fix — not just a style preference. */
    char error_buf[CURL_ERROR_SIZE] = {0};

    CURL *easy = curl_easy_init();
    if (!easy) {
        result.error = "curl_easy_init failed";
    } else {
        curl_easy_setopt(easy, CURLOPT_URL, job->url);
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(easy, CURLOPT_WRITEDATA, &body);
        curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, error_buf);
        curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(easy, CURLOPT_MAXREDIRS, 10L);
        curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS, 30000L);
        curl_easy_setopt(easy, CURLOPT_ACCEPT_ENCODING, ""); /* enable all
            supported decompression (gzip/br/deflate) — an empty string,
            not NULL, is curl's documented way to request "whatever this
            build supports" rather than a specific single encoding. */
        curl_easy_setopt(easy, CURLOPT_USERAGENT, "causality-browser/0.1");
        /* Documented safe default for any multi-threaded application
           regardless of resolver backend — see CURLOPT_NOSIGNAL's docs. */
        curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);

        CURLcode rc = curl_easy_perform(easy);

        if (rc != CURLE_OK) {
            result.error = error_buf[0] ? error_buf : curl_easy_strerror(rc);
        } else {
            long status = 0;
            curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &status);

            char *content_type = NULL;
            curl_easy_getinfo(easy, CURLINFO_CONTENT_TYPE, &content_type);
            if (content_type) {
                size_t ct_len = strlen(content_type);
                content_type_copy = malloc(ct_len + 1);
                if (content_type_copy) memcpy(content_type_copy, content_type, ct_len + 1);
            }

            result.ok          = true;
            result.status       = (int)status;
            result.body         = body.data ? body.data : "";
            result.body_len     = body.len;
            result.content_type = content_type_copy;
        }

        curl_easy_cleanup(easy);
    }

    if (job->on_complete) job->on_complete(&result, job->user_data);
    if (job->events)
        eng_event_publish(job->events, ENG_NET_EVENT_FETCH_COMPLETE,
                          &result, sizeof(result));

    free(content_type_copy);
    free(body.data);
    free(job->url);
    free(job);
}

Eng_NetSystem *eng_net_system_create(void)
{
    CURLcode rc = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (rc != CURLE_OK) {
        ENG_LOG_ERROR("net", "curl_global_init failed: %s", curl_easy_strerror(rc));
        return NULL;
    }

    Eng_NetSystem *net = calloc(1, sizeof(Eng_NetSystem));
    if (!net) {
        curl_global_cleanup();
        return NULL;
    }
    return net;
}

void eng_net_system_destroy(Eng_NetSystem *net)
{
    if (!net) return;
    free(net);
    curl_global_cleanup();
}

void eng_net_fetch(Eng_NetSystem *net, Eng_JobSystem *jobs,
                    const char *url,
                    Eng_NetFetchFn on_complete, void *user_data,
                    Eng_EventBus *events, Eng_JobCounter *counter)
{
    if (!net || !jobs || !url) return;

    FetchJob *job = calloc(1, sizeof(FetchJob));
    if (!job) return;

    size_t url_len = strlen(url);
    job->url = malloc(url_len + 1);
    if (!job->url) {
        free(job);
        return;
    }
    memcpy(job->url, url, url_len + 1);

    job->net         = net;
    job->on_complete = on_complete;
    job->user_data   = user_data;
    job->events      = events;

    eng_job_submit(jobs, run_fetch, job, counter);
}
