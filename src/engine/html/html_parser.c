// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#include "html_parser.h"
#include "../core/logger.h"

#include <gumbo.h>
#include <stdlib.h>

struct Eng_HtmlDocument {
    GumboOutput *output;
};

/* Eng_HtmlNode is GumboNode by another name — gumbo's tree already has
   stable node pointers for the document's lifetime, so wrapping each node
   in its own heap allocation would only add cost without adding safety.
   The opaque typedef in the header still keeps GumboNode out of every
   call site outside this file. */
static const GumboNode *as_gumbo(const Eng_HtmlNode *node)
{
    return (const GumboNode *)node;
}

static Eng_HtmlNode *from_gumbo(GumboNode *node)
{
    return (Eng_HtmlNode *)node;
}

Eng_HtmlDocument *eng_html_parse(const char *source, size_t source_len)
{
    Eng_HtmlDocument *doc = calloc(1, sizeof(Eng_HtmlDocument));
    if (!doc) return NULL;

    doc->output = gumbo_parse_with_options(&kGumboDefaultOptions,
                                            source, source_len);
    if (!doc->output) {
        /* gumbo_parse_with_options only returns NULL on internal
           allocation failure — malformed markup is recovered from per
           the HTML5 spec and never reaches this branch. */
        ENG_LOG_ERROR("html", "gumbo_parse_with_options failed");
        free(doc);
        return NULL;
    }

    return doc;
}

void eng_html_document_destroy(Eng_HtmlDocument *doc)
{
    if (!doc) return;
    gumbo_destroy_output(&kGumboDefaultOptions, doc->output);
    free(doc);
}

Eng_HtmlNode *eng_html_document_root(const Eng_HtmlDocument *doc)
{
    if (!doc) return NULL;
    return from_gumbo(doc->output->root);
}

Eng_HtmlNodeType eng_html_node_type(const Eng_HtmlNode *node)
{
    if (!node) return ENG_HTML_NODE_TEXT;

    switch (as_gumbo(node)->type) {
        case GUMBO_NODE_ELEMENT:
        case GUMBO_NODE_TEMPLATE:
            return ENG_HTML_NODE_ELEMENT;
        case GUMBO_NODE_COMMENT:
            return ENG_HTML_NODE_COMMENT;
        case GUMBO_NODE_DOCUMENT:
        case GUMBO_NODE_TEXT:
        case GUMBO_NODE_CDATA:
        case GUMBO_NODE_WHITESPACE:
        default:
            return ENG_HTML_NODE_TEXT;
    }
}

const char *eng_html_node_tag_name(const Eng_HtmlNode *node)
{
    if (!node) return NULL;
    const GumboNode *g = as_gumbo(node);
    if (g->type != GUMBO_NODE_ELEMENT && g->type != GUMBO_NODE_TEMPLATE)
        return NULL;

    if (g->v.element.tag == GUMBO_TAG_UNKNOWN) {
        /* Custom/unrecognized element — fall back to the literal source
           text (e.g. "my-widget") since gumbo has no normalized name
           for it. original_tag includes the angle brackets and any
           attributes, so this is only correct for a bare "<tag" match;
           good enough for logging/diagnostics, not for a real tag-name
           string. Real DOM-bridge code should special-case this rather
           than trust it as a clean tag name. */
        return g->v.element.original_tag.data
                   ? g->v.element.original_tag.data
                   : "";
    }
    return gumbo_normalized_tagname(g->v.element.tag);
}

const char *eng_html_node_text(const Eng_HtmlNode *node)
{
    if (!node) return NULL;
    const GumboNode *g = as_gumbo(node);
    switch (g->type) {
        case GUMBO_NODE_TEXT:
        case GUMBO_NODE_CDATA:
        case GUMBO_NODE_WHITESPACE:
        case GUMBO_NODE_COMMENT:
            return g->v.text.text;
        default:
            return NULL;
    }
}

int eng_html_node_child_count(const Eng_HtmlNode *node)
{
    if (!node) return 0;
    const GumboNode *g = as_gumbo(node);
    if (g->type != GUMBO_NODE_ELEMENT && g->type != GUMBO_NODE_TEMPLATE)
        return 0;
    return (int)g->v.element.children.length;
}

Eng_HtmlNode *eng_html_node_child(const Eng_HtmlNode *node, int index)
{
    if (!node || index < 0) return NULL;
    const GumboNode *g = as_gumbo(node);
    if (g->type != GUMBO_NODE_ELEMENT && g->type != GUMBO_NODE_TEMPLATE)
        return NULL;
    if ((size_t)index >= g->v.element.children.length) return NULL;

    return from_gumbo((GumboNode *)g->v.element.children.data[index]);
}

int eng_html_node_attribute_count(const Eng_HtmlNode *node)
{
    if (!node) return 0;
    const GumboNode *g = as_gumbo(node);
    if (g->type != GUMBO_NODE_ELEMENT && g->type != GUMBO_NODE_TEMPLATE)
        return 0;
    return (int)g->v.element.attributes.length;
}

Eng_HtmlAttribute eng_html_node_attribute(const Eng_HtmlNode *node, int index)
{
    Eng_HtmlAttribute result = { .name = NULL, .value = NULL };
    if (!node || index < 0) return result;

    const GumboNode *g = as_gumbo(node);
    if (g->type != GUMBO_NODE_ELEMENT && g->type != GUMBO_NODE_TEMPLATE)
        return result;
    if ((size_t)index >= g->v.element.attributes.length) return result;

    const GumboAttribute *attr =
        (const GumboAttribute *)g->v.element.attributes.data[index];
    result.name  = attr->name;
    result.value = attr->value;
    return result;
}

const char *eng_html_node_attribute_value(const Eng_HtmlNode *node,
                                           const char *name)
{
    if (!node || !name) return NULL;
    const GumboNode *g = as_gumbo(node);
    if (g->type != GUMBO_NODE_ELEMENT && g->type != GUMBO_NODE_TEMPLATE)
        return NULL;

    GumboAttribute *attr = gumbo_get_attribute(&g->v.element.attributes, name);
    return attr ? attr->value : NULL;
}
