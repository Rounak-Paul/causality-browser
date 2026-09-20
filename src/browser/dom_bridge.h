// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#pragma once

#include "../engine/html/html_parser.h"

/* Forward-declared rather than pulling in causality.h here: only the
   pointer type is needed for eng_dom_render_builder's signature, and
   every call site already includes causality.h for its own UI calls. */
typedef struct Ca_Node Ca_Div;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Walks a parsed HTML document's <body> and emits the equivalent
 * causality UI tree via ca_div_begin/ca_text/ca_image/etc.
 *
 * Pure function, no state kept across calls: causality's UI builder is
 * immediate-mode (the whole tree is rebuilt from scratch on every
 * ca_ui_begin/ca_ui_end or ca_div_set_builder rebuild), so this simply
 * re-walks the DOM and re-emits every call each time it runs. Must be
 * called with a causality div already open (e.g. from inside a
 * ca_div_set_builder callback, or directly between ca_ui_begin/
 * ca_ui_end) — it emits children into whatever div is currently on top
 * of causality's parent stack, it does not open one of its own.
 *
 * CSS is NOT resolved by this function. Each emitted node carries the
 * source element's id/class straight through to Ca_DivDesc.id/.style
 * (causality's builder already resolves those against whatever
 * Ca_Stylesheet is attached to the instance via
 * ca_instance_set_stylesheet — see ca_style_resolve_layers, called
 * internally by ca_div_begin/ca_text/etc.). Callers are responsible for
 * parsing the page's CSS (inline <style>, linked stylesheets) with
 * ca_css_parse and attaching it before this runs.
 *
 * doc  Parsed document to render; must outlive the call (the walk reads
 *      attribute/text strings directly out of it without copying).
 */
void eng_dom_render(const Eng_HtmlDocument *doc);

/**
 * ca_div_set_builder-compatible wrapper around eng_dom_render — pass this
 * directly as the builder fn with an Eng_HtmlDocument* as user_data:
 *
 *   ca_div_set_builder(content_div, eng_dom_render_builder, doc);
 *
 * div        Ignored (causality has already cleared and entered it by the
 *            time a builder runs — see ca_div_set_builder's contract).
 * user_data  The Eng_HtmlDocument* to render.
 */
void eng_dom_render_builder(Ca_Div *div, void *user_data);

/**
 * Default "user-agent" CSS for tags eng_dom_render emits as
 * "tag-<name>" classes (see render_inline_text in dom_bridge.c) — bold
 * for <b>/<strong>, italic-style color for <i>/<em>, a link color for
 * <a>, etc. A real browser ships an equivalent built-in stylesheet
 * layered beneath the page's own CSS.
 *
 * Callers should ca_css_parse this alongside (or merged with) the page's
 * own stylesheet and attach both to the instance — causality resolves
 * classes against whatever single Ca_Stylesheet is currently attached
 * via ca_instance_set_stylesheet, so the caller is responsible for
 * combining this constant with page CSS into one parsed sheet (e.g. by
 * concatenating the two source strings before calling ca_css_parse, or
 * whatever composition strategy the caller's stylesheet-layering design
 * settles on).
 */
extern const char *eng_dom_default_css;

#ifdef __cplusplus
}
#endif
