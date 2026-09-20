// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#include "dom_bridge.h"
#include "../engine/core/logger.h"

#include <causality.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* How a tag is rendered. Kept deliberately small — extend per-tag as real
   pages expose gaps, rather than trying to anticipate every HTML element
   up front. */
typedef enum TagKind {
    TAG_BLOCK,      /* ca_div_begin/end with .inline_flow set — its own
                        direct children (text + inline elements) wrap
                        together via causality's inline formatting
                        context (Ca_DivDesc.inline_flow) instead of
                        flexbox */
    TAG_INLINE,     /* a run inside an inline_flow block: emitted as its
                        own ca_text (if it carries text) so causality's
                        inline layout picks it up as a separate word run
                        with its own font/color; see render_inline_text */
    TAG_IMAGE,      /* placeholder box — see render_element's IMG case */
    TAG_BREAK,      /* <br> — emits a line-height spacer, no children */
    TAG_SKIP,       /* non-rendering: parsed but contributes no visual node,
                        e.g. <head>, <script>, <style> (script/style text
                        content is data, not document text — must not be
                        walked as if it were prose) */
} TagKind;

static TagKind tag_kind_for(const char *tag)
{
    if (!tag) return TAG_SKIP;

    /* Structural/no-render */
    if (strcmp(tag, "head") == 0 || strcmp(tag, "script") == 0 ||
        strcmp(tag, "style") == 0 || strcmp(tag, "title") == 0 ||
        strcmp(tag, "meta") == 0 || strcmp(tag, "link") == 0 ||
        strcmp(tag, "noscript") == 0 || strcmp(tag, "template") == 0)
        return TAG_SKIP;

    if (strcmp(tag, "img") == 0) return TAG_IMAGE;
    if (strcmp(tag, "br") == 0)  return TAG_BREAK;

    /* Inline elements. Not exhaustive; unrecognized tags fall through to
       TAG_BLOCK below, which is the safer default (worst case: an inline
       element gets an unwanted line break, vs. a block element losing
       its layout entirely). */
    if (strcmp(tag, "span") == 0 || strcmp(tag, "a") == 0 ||
        strcmp(tag, "b") == 0 || strcmp(tag, "strong") == 0 ||
        strcmp(tag, "i") == 0 || strcmp(tag, "em") == 0 ||
        strcmp(tag, "small") == 0 || strcmp(tag, "label") == 0 ||
        strcmp(tag, "code") == 0)
        return TAG_INLINE;

    /* Everything else (div, p, section, article, header, footer, nav,
       ul/li, h1-h6, body, html, form, button, etc.) — block by default. */
    return TAG_BLOCK;
}

/* Collects an inline element's own text into one flat buffer — e.g.
   <b>bold <i>and italic</i></b> becomes one "bold and italic" run.
   Causality's inline_flow layout (vendors/causality) only understands
   flat CA_WIDGET_LABEL children of an inline_flow container, not nested
   inline sub-containers (see inline_layout.c's doc comment on that first-
   implementation scope) — so nested inline markup is flattened to plain
   text rather than dropped or mis-rendered as a separate block. Losing
   e.g. "italic within bold" as a distinct style is an accepted
   simplification for this first pass, not an oversight. */
static void collect_inline_text(const Eng_HtmlNode *node, char *out, size_t out_size)
{
    if (!node || out_size == 0) return;

    if (eng_html_node_type(node) == ENG_HTML_NODE_TEXT) {
        const char *text = eng_html_node_text(node);
        if (text) {
            size_t used = strlen(out);
            if (used < out_size - 1)
                strncat(out, text, out_size - used - 1);
        }
        return;
    }
    if (eng_html_node_type(node) != ENG_HTML_NODE_ELEMENT) return;

    int count = eng_html_node_child_count(node);
    for (int i = 0; i < count; ++i)
        collect_inline_text(eng_html_node_child(node, i), out, out_size);
}

static bool is_blank(const char *text)
{
    if (!text) return true;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p)
        if (!isspace(*p)) return false;
    return true;
}

static void render_node(const Eng_HtmlNode *node);

static void render_children(const Eng_HtmlNode *node)
{
    int count = eng_html_node_child_count(node);
    for (int i = 0; i < count; ++i)
        render_node(eng_html_node_child(node, i));
}

/* True if every non-blank child of `node` is plain text or a TAG_INLINE
   element — i.e. this block's content is prose that should wrap as one
   inline formatting context (causality's Ca_DivDesc.inline_flow), the
   same rule a real browser uses to decide a <p>'s children flow inline
   while a <ul>'s <li> children each get their own block box. A block
   with even one block-level child (or no children at all) renders via
   plain flexbox instead. */
static bool has_only_inline_content(const Eng_HtmlNode *node)
{
    int count = eng_html_node_child_count(node);
    bool saw_content = false;
    for (int i = 0; i < count; ++i) {
        const Eng_HtmlNode *child = eng_html_node_child(node, i);
        Eng_HtmlNodeType type = eng_html_node_type(child);
        if (type == ENG_HTML_NODE_TEXT) {
            if (!is_blank(eng_html_node_text(child))) saw_content = true;
            continue;
        }
        if (type == ENG_HTML_NODE_COMMENT) continue;
        /* ENG_HTML_NODE_ELEMENT */
        TagKind child_kind = tag_kind_for(eng_html_node_tag_name(child));
        if (child_kind == TAG_SKIP) continue;
        if (child_kind != TAG_INLINE && child_kind != TAG_BREAK) return false;
        saw_content = true;
    }
    return saw_content;
}

/* Renders one TAG_INLINE element's collected text as a single ca_text
   run, styled by a "tag-<name>" class (e.g. "tag-b", "tag-a") so a
   default stylesheet can give <b> bold weight, <a> a link color, etc. —
   see eng_dom_default_css. Must be called only from inside an
   inline_flow container (render_element's TAG_BLOCK branch). */
static void render_inline_text(const Eng_HtmlNode *node, const char *tag)
{
    char buf[1024] = {0};
    collect_inline_text(node, buf, sizeof(buf));
    if (buf[0] == '\0') return;

    char class_buf[80];
    const char *user_class = eng_html_node_attribute_value(node, "class");
    if (user_class && user_class[0] != '\0')
        snprintf(class_buf, sizeof(class_buf), "tag-%s %s", tag, user_class);
    else
        snprintf(class_buf, sizeof(class_buf), "tag-%s", tag);

    ca_text(&(Ca_TextDesc){
        .text  = buf,
        .id    = eng_html_node_attribute_value(node, "id"),
        .style = class_buf,
    });
}

/* Emits every child of `node` as flat inline runs (ca_text calls) —
   shared by any block whose content is prose (has_only_inline_content):
   a normal <p>/<h1>/etc. and an <li>'s own content both use this. Must
   be called with an inline_flow div already open (ca_div_begin), since
   causality's ca_inline_layout expects flat CA_WIDGET_LABEL children,
   one per run, not a nested div per inline element. */
static void render_inline_content(const Eng_HtmlNode *node)
{
    int count = eng_html_node_child_count(node);
    for (int i = 0; i < count; ++i) {
        const Eng_HtmlNode *child = eng_html_node_child(node, i);
        Eng_HtmlNodeType type = eng_html_node_type(child);
        if (type == ENG_HTML_NODE_TEXT) {
            const char *text = eng_html_node_text(child);
            if (!is_blank(text)) ca_text(&(Ca_TextDesc){ .text = text });
        } else if (type == ENG_HTML_NODE_ELEMENT) {
            TagKind child_kind = tag_kind_for(eng_html_node_tag_name(child));
            if (child_kind == TAG_BREAK) {
                /* A hard line break mid-paragraph: not representable as
                   an inline run in this first implementation (see
                   inline_layout.c's scope) — silently omitted rather
                   than mis-rendered. Accepted simplification. */
                continue;
            }
            if (child_kind == TAG_INLINE)
                render_inline_text(child, eng_html_node_tag_name(child));
        }
    }
}

static void render_element(const Eng_HtmlNode *node)
{
    const char *tag = eng_html_node_tag_name(node);
    TagKind kind = tag_kind_for(tag);

    if (kind == TAG_SKIP) return;

    /* id/class pass straight through to causality's own cascade —
       ca_div_begin/ca_text resolve these against whatever Ca_Stylesheet
       is attached to the instance (ca_style_resolve_layers, called
       internally); this bridge does no selector matching itself. */
    const char *id    = eng_html_node_attribute_value(node, "id");
    const char *class = eng_html_node_attribute_value(node, "class");

    if (kind == TAG_BREAK) {
        ca_spacer(&(Ca_SpacerDesc){ .height = 16 });
        return;
    }

    if (kind == TAG_IMAGE) {
        /* No decoder/network fetch yet (see .context/browser-dom-bridge.md)
           — render a fixed placeholder box so layout doesn't collapse
           around a missing image, rather than silently dropping it. */
        const char *alt = eng_html_node_attribute_value(node, "alt");
        ca_div_begin(&(Ca_DivDesc){
            .id = id, .style = class,
            .width = 32, .height = 32,
            .background = ca_color(0.85f, 0.85f, 0.85f, 1.0f),
        });
        if (alt && alt[0] != '\0')
            ca_text(&(Ca_TextDesc){ .text = alt });
        ca_div_end();
        return;
    }

    if (kind == TAG_INLINE) {
        /* Reached directly only when an inline element appears outside
           any inline_flow ancestor (e.g. a bare top-level <b>text</b>)
           — the normal case (inline content inside a <p>) is handled by
           render_children's inline_flow branch below without ever
           calling render_element on the inline child itself. Fall back
           to a block box so the content is still visible. */
        Ca_DivDesc desc = { .id = id, .style = class, .direction = CA_HORIZONTAL };
        ca_div_begin(&desc);
        render_children(node);
        ca_div_end();
        return;
    }

    /* <ul>/<ol>: indent the whole list; causality's list primitives
       (ca_list_begin/ca_li_begin) are plain divs with no built-in
       marker glyph or default indentation (verified — see
       vendors/causality/causality/src/ui/widget.c), so both are
       supplied here rather than relying on causality to draw them. */
    if (tag && (strcmp(tag, "ul") == 0 || strcmp(tag, "ol") == 0)) {
        ca_div_begin(&(Ca_DivDesc){
            .id = id, .style = class,
            .direction = CA_VERTICAL, .gap = 2,
            .padding = { 0, 0, 0, 20 },
        });
        render_children(node);
        ca_div_end();
        return;
    }

    /* <li>: a literal bullet glyph + content, side by side. Ordered-list
       numbering (<ol>) is not distinguished from unordered (<ul>) yet —
       both render a bullet; real numbering would need this element's
       index among its <ol> parent's <li> siblings threaded through, not
       attempted in this first pass. */
    if (tag && strcmp(tag, "li") == 0) {
        ca_div_begin(&(Ca_DivDesc){
            .id = id, .style = class,
            .direction = CA_HORIZONTAL, .gap = 8,
        });
        ca_text(&(Ca_TextDesc){ .text = "\xE2\x80\xA2" }); /* U+2022 BULLET */
        bool li_inline = has_only_inline_content(node);
        ca_div_begin(&(Ca_DivDesc){ .direction = CA_VERTICAL, .inline_flow = li_inline });
        if (li_inline) render_inline_content(node);
        else            render_children(node);
        ca_div_end();
        ca_div_end();
        return;
    }

    bool inline_flow = has_only_inline_content(node);
    Ca_DivDesc desc = {
        .id = id, .style = class,
        .direction = CA_VERTICAL,
        .inline_flow = inline_flow,
    };
    ca_div_begin(&desc);
    if (inline_flow) render_inline_content(node);
    else              render_children(node);
    ca_div_end();
}

static void render_node(const Eng_HtmlNode *node)
{
    if (!node) return;

    switch (eng_html_node_type(node)) {
        case ENG_HTML_NODE_ELEMENT:
            render_element(node);
            return;
        case ENG_HTML_NODE_TEXT: {
            const char *text = eng_html_node_text(node);
            if (!is_blank(text))
                ca_text(&(Ca_TextDesc){ .text = text, .wrap = true });
            return;
        }
        case ENG_HTML_NODE_COMMENT:
            return; /* never rendered */
    }
}

/* Depth-first search for the first descendant element with the given tag
   name (used to locate <body> under the parsed <html> root — gumbo
   always synthesizes both regardless of what the source actually
   contained, per HTML5 error-recovery rules). */
static const Eng_HtmlNode *find_first(const Eng_HtmlNode *node, const char *tag)
{
    if (!node) return NULL;
    if (eng_html_node_type(node) == ENG_HTML_NODE_ELEMENT) {
        const char *node_tag = eng_html_node_tag_name(node);
        if (node_tag && strcmp(node_tag, tag) == 0) return node;
    }
    int count = eng_html_node_child_count(node);
    for (int i = 0; i < count; ++i) {
        const Eng_HtmlNode *found = find_first(eng_html_node_child(node, i), tag);
        if (found) return found;
    }
    return NULL;
}

void eng_dom_render(const Eng_HtmlDocument *doc)
{
    if (!doc) return;

    Eng_HtmlNode *root = eng_html_document_root(doc);
    const Eng_HtmlNode *body = find_first(root, "body");
    if (!body) {
        ENG_LOG_WARN("dom", "no <body> found in document, nothing to render");
        return;
    }

    render_children(body);
}

void eng_dom_render_builder(Ca_Div *div, void *user_data)
{
    (void)div; /* already cleared/entered by causality before this runs */
    eng_dom_render((const Eng_HtmlDocument *)user_data);
}

const char *eng_dom_default_css =
    ".tag-b, .tag-strong { font-weight: bold; }"
    ".tag-a              { color: #4285f4; }"
    ".tag-code           { color: #d14; }";
/* No font-style (italic) equivalent: causality's CA_CSS_PROP_FONT_STYLE
   is parsed (css.h/css.c) but font.h's Ca_FontTier selection only
   branches on font_bold, not an italic axis — there is no italic glyph
   tier to select even if font-style were threaded through to
   Ca_NodeDesc the way font-weight is. <i>/<em> render as plain-weight,
   non-bold text for now; a real fix needs an italic font variant loaded
   into the font system, out of scope for this pass. */
