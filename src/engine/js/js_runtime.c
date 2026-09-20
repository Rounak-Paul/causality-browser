// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#include "js_runtime.h"
#include "../core/logger.h"

#include <quickjs.h>
#include <stdlib.h>
#include <string.h>

struct Eng_JsRuntime {
    JSRuntime *rt;
};

struct Eng_JsContext {
    JSContext *ctx;
};

Eng_JsRuntime *eng_js_runtime_create(size_t memory_limit_bytes)
{
    Eng_JsRuntime *runtime = calloc(1, sizeof(Eng_JsRuntime));
    if (!runtime) return NULL;

    runtime->rt = JS_NewRuntime();
    if (!runtime->rt) {
        ENG_LOG_ERROR("js", "JS_NewRuntime failed");
        free(runtime);
        return NULL;
    }

    if (memory_limit_bytes > 0)
        JS_SetMemoryLimit(runtime->rt, memory_limit_bytes);

    return runtime;
}

void eng_js_runtime_destroy(Eng_JsRuntime *runtime)
{
    if (!runtime) return;
    JS_FreeRuntime(runtime->rt);
    free(runtime);
}

int eng_js_runtime_run_jobs(Eng_JsRuntime *runtime)
{
    if (!runtime) return 0;

    int executed = 0;
    JSContext *job_ctx;
    for (;;) {
        int ret = JS_ExecutePendingJob(runtime->rt, &job_ctx);
        if (ret <= 0) {
            if (ret < 0) {
                /* A job threw; quickjs-ng has no return path for the
                   exception here (job_ctx owns it) — log and keep
                   draining so one bad promise reaction doesn't stall
                   every other queued job. */
                JSValue exc = JS_GetException(job_ctx);
                const char *msg = JS_ToCString(job_ctx, exc);
                ENG_LOG_ERROR("js", "unhandled job exception: %s",
                               msg ? msg : "(unstringifiable)");
                if (msg) JS_FreeCString(job_ctx, msg);
                JS_FreeValue(job_ctx, exc);
            }
            break;
        }
        ++executed;
    }
    return executed;
}

void eng_js_runtime_collect_garbage(Eng_JsRuntime *runtime)
{
    if (!runtime) return;
    JS_RunGC(runtime->rt);
}

Eng_JsContext *eng_js_context_create(Eng_JsRuntime *runtime)
{
    if (!runtime) return NULL;

    Eng_JsContext *context = calloc(1, sizeof(Eng_JsContext));
    if (!context) return NULL;

    context->ctx = JS_NewContext(runtime->rt);
    if (!context->ctx) {
        ENG_LOG_ERROR("js", "JS_NewContext failed");
        free(context);
        return NULL;
    }

    return context;
}

void eng_js_context_destroy(Eng_JsContext *context)
{
    if (!context) return;
    JS_FreeContext(context->ctx);
    free(context);
}

static char *dup_cstring(const char *s)
{
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    if (copy) memcpy(copy, s, len + 1);
    return copy;
}

Eng_JsResult eng_js_eval(Eng_JsContext *context, const char *source,
                          size_t source_len, const char *filename)
{
    Eng_JsResult result = { .ok = false, .value = NULL, .error = NULL };
    if (!context) {
        result.error = dup_cstring("null context");
        return result;
    }

    JSValue completion = JS_Eval(context->ctx, source, source_len,
                                  filename ? filename : "<script>",
                                  JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(completion)) {
        JSValue exc = JS_GetException(context->ctx);
        const char *msg = JS_ToCString(context->ctx, exc);
        result.error = dup_cstring(msg ? msg : "unknown JS exception");
        if (msg) JS_FreeCString(context->ctx, msg);
        JS_FreeValue(context->ctx, exc);
    } else {
        const char *str = JS_ToCString(context->ctx, completion);
        result.ok    = true;
        result.value = dup_cstring(str ? str : "");
        if (str) JS_FreeCString(context->ctx, str);
    }

    JS_FreeValue(context->ctx, completion);
    return result;
}

void eng_js_result_free(Eng_JsResult *result)
{
    if (!result) return;
    free(result->value);
    free(result->error);
    result->value = NULL;
    result->error = NULL;
    result->ok    = false;
}
