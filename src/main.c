// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#include <causality.h>
#include <string.h>

#include "engine/engine.h"
#include "engine/html/html_parser.h"

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

/* Recursively logs an element/text tree, proving child + attribute
   traversal works end to end rather than just top-level parse success. */
static void log_html_tree(const Eng_HtmlNode *node, int depth)
{
    if (!node) return;

    char indent[64];
    int  pad = depth * 2 < (int)sizeof(indent) - 1 ? depth * 2 : (int)sizeof(indent) - 1;
    memset(indent, ' ', (size_t)pad);
    indent[pad] = '\0';

    if (eng_html_node_type(node) == ENG_HTML_NODE_ELEMENT) {
        const char *tag = eng_html_node_tag_name(node);
        int attr_count = eng_html_node_attribute_count(node);
        if (attr_count > 0) {
            Eng_HtmlAttribute attr = eng_html_node_attribute(node, 0);
            ENG_LOG_INFO("html", "%s<%s %s=\"%s\"> (%d attr, %d children)",
                         indent, tag, attr.name, attr.value, attr_count,
                         eng_html_node_child_count(node));
        } else {
            ENG_LOG_INFO("html", "%s<%s> (%d children)", indent, tag,
                         eng_html_node_child_count(node));
        }
        for (int i = 0; i < eng_html_node_child_count(node); ++i)
            log_html_tree(eng_html_node_child(node, i), depth + 1);
    } else if (eng_html_node_type(node) == ENG_HTML_NODE_TEXT) {
        const char *text = eng_html_node_text(node);
        if (text && text[0] != '\0')
            ENG_LOG_INFO("html", "%s\"%s\"", indent, text);
    }
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

    /* JS smoke test: one context (the eventual per-tab scope), evaluate
       a trivial expression, log the result. */
    Eng_JsContext *js_ctx = eng_js_context_create(engine.js);
    if (js_ctx) {
        static const char *probe_src = "1 + 2 * 3";
        Eng_JsResult js_result = eng_js_eval(js_ctx, probe_src,
                                              strlen(probe_src), "<startup-probe>");
        if (js_result.ok)
            ENG_LOG_INFO("app", "JS probe result: %s", js_result.value);
        else
            ENG_LOG_ERROR("app", "JS probe failed: %s", js_result.error);
        eng_js_result_free(&js_result);
        eng_js_context_destroy(js_ctx);
    } else {
        ENG_LOG_ERROR("app", "failed to create JS context");
    }

    /* HTML smoke test: parse a small fragment (including a deliberately
       unclosed tag, to confirm gumbo's error-recovery path runs cleanly
       through the wrapper) and walk the resulting tree. */
    {
        static const char *html_src =
            "<html><body>"
            "<h1 id=\"title\">Hello</h1>"
            "<p class=\"intro\">World <b>bold</b></p>"
            "<ul><li>One<li>Two</ul>"
            "</body></html>";
        Eng_HtmlDocument *doc = eng_html_parse(html_src, strlen(html_src));
        if (doc) {
            ENG_LOG_INFO("app", "HTML probe: parsed, walking tree");
            log_html_tree(eng_html_document_root(doc), 0);
            eng_html_document_destroy(doc);
        } else {
            ENG_LOG_ERROR("app", "HTML probe failed to parse");
        }
    }

    while (ca_instance_tick(instance)) {
        eng_js_runtime_run_jobs(engine.js);
    }

    ca_instance_destroy(instance);
    eng_engine_shutdown(&engine);
    return 0;
}
