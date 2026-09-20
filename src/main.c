// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#include <causality.h>
#include <stdio.h>
#include <string.h>

#include "engine/engine.h"
#include "engine/html/html_parser.h"
#include "browser/dom_bridge.h"

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

    /* HTML tree-walk sanity check (log-only, kept from the parser wrapper
       milestone) before the real render pass below. */
    {
        static const char *html_src =
            "<html><body>"
            "<h1 id=\"title\">Hello</h1>"
            "<p class=\"intro\">World <b>bold</b></p>"
            "<ul><li>One<li>Two</ul>"
            "</body></html>";
        Eng_HtmlDocument *check_doc = eng_html_parse(html_src, strlen(html_src));
        if (check_doc) {
            ENG_LOG_INFO("app", "HTML probe: parsed, walking tree");
            log_html_tree(eng_html_document_root(check_doc), 0);
            eng_html_document_destroy(check_doc);
        } else {
            ENG_LOG_ERROR("app", "HTML probe failed to parse");
        }
    }

    /* DOM->causality render bridge demo: a small static page (headings,
       paragraphs, a list, a link, an image placeholder) styled with a
       real parsed stylesheet, rendered through eng_dom_render. No
       network fetch or JS-driven DOM mutation yet — see
       .context/browser-dom-bridge.md for what's still missing before
       this can point at a live URL. */
    static const char *page_css =
        ".body { background: #202124; padding: 24px; gap: 12px; }"
        "h1 { color: #e8eaed; }"
        "p { color: #bdc1c6; }"
        ".intro { color: #8ab4f8; }";
    /* eng_dom_default_css supplies the "tag-b"/"tag-a"/etc. rules that
       give inline elements their expected weight/color (see
       dom_bridge.h) — concatenated with the page's own CSS into one
       source string since causality resolves classes against exactly
       one attached Ca_Stylesheet at a time (ca_instance_set_stylesheet
       replaces, not layers). */
    char combined_css[2048];
    snprintf(combined_css, sizeof(combined_css), "%s%s",
             eng_dom_default_css, page_css);
    Ca_Stylesheet *page_sheet = ca_css_parse(combined_css);
    if (page_sheet) ca_instance_set_stylesheet(instance, page_sheet);

    static const char *page_html =
        "<html><body>"
        "<h1>Causality Browser</h1>"
        "<p class=\"intro\">Rendered from HTML via the DOM bridge — no network yet.</p>"
        "<p>This paragraph, the list below, and the image placeholder all "
        "came from <a href=\"#\">gumbo-parsed</a> markup, walked by "
        "<b>eng_dom_render</b> and emitted as real causality UI calls.</p>"
        "<ul><li>Headings and paragraphs</li><li>Inline elements (bold, links)</li>"
        "<li>Image placeholders (no decoder/fetch yet)</li></ul>"
        "<img alt=\"demo placeholder\">"
        "</body></html>";
    Eng_HtmlDocument *page_doc = eng_html_parse(page_html, strlen(page_html));
    if (!page_doc) {
        ENG_LOG_FATAL("app", "failed to parse demo page");
        ca_instance_destroy(instance);
        eng_engine_shutdown(&engine);
        return 1;
    }

    ca_ui_begin(window, &(Ca_DivDesc){ .direction = CA_VERTICAL, .style = "body" });
        Ca_Div *content = ca_div_begin(&(Ca_DivDesc){ .direction = CA_VERTICAL, .gap = 8 });
        ca_div_set_builder(content, eng_dom_render_builder, page_doc);
        ca_div_end();
    ca_ui_end();

    while (ca_instance_tick(instance)) {
        eng_js_runtime_run_jobs(engine.js);
    }

    eng_html_document_destroy(page_doc);
    if (page_sheet) ca_css_destroy(page_sheet);
    ca_instance_destroy(instance);
    eng_engine_shutdown(&engine);
    return 0;
}
