// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Owns one parsed document's node tree. Wraps gumbo-parser's GumboOutput;
    the raw type never crosses this header. Free with
    eng_html_document_destroy once done walking it — every Eng_HtmlNode
    obtained from it becomes invalid at that point. */
typedef struct Eng_HtmlDocument Eng_HtmlDocument;

/** Read-only handle into a node inside an Eng_HtmlDocument's tree. Valid
    only while the owning document is alive. */
typedef struct Eng_HtmlNode Eng_HtmlNode;

typedef enum Eng_HtmlNodeType {
    ENG_HTML_NODE_ELEMENT,    /* a tag, e.g. <div>, <p>, <a> */
    ENG_HTML_NODE_TEXT,       /* decoded text content between tags */
    ENG_HTML_NODE_COMMENT,    /* <!-- ... --> */
} Eng_HtmlNodeType;

/** One attribute on an element node, e.g. href="https://...". */
typedef struct Eng_HtmlAttribute {
    const char *name;
    const char *value;
} Eng_HtmlAttribute;

/**
 * Parses an HTML document (full HTML5 tree-construction algorithm,
 * including tag-soup error recovery — malformed markup never fails this
 * call, it degrades the same way a browser's own parser would).
 *
 * source      HTML source bytes; need not be null-terminated at source_len.
 * source_len  Length of source in bytes.
 * Returns     Heap-allocated document, or NULL only on allocation failure
 *             (not on malformed markup — see above). Free with
 *             eng_html_document_destroy.
 */
Eng_HtmlDocument *eng_html_parse(const char *source, size_t source_len);

/** Destroys a parsed document and every node/attribute view obtained from
    it. */
void eng_html_document_destroy(Eng_HtmlDocument *doc);

/** Returns the document's root <html> element node. Present even for
    fragmentary/malformed input — gumbo always synthesizes html/head/body
    per the HTML5 spec's error-recovery rules. */
Eng_HtmlNode *eng_html_document_root(const Eng_HtmlDocument *doc);

/** Returns the node's type; determines which of the accessors below are
    valid to call on it. */
Eng_HtmlNodeType eng_html_node_type(const Eng_HtmlNode *node);

/**
 * Returns an element node's normalized (lowercase) tag name, e.g. "div",
 * "a", "script". For a tag gumbo doesn't recognize (a custom element),
 * returns the literal tag text as written in the source.
 *
 * node  Element node (ENG_HTML_NODE_ELEMENT); other types return NULL.
 */
const char *eng_html_node_tag_name(const Eng_HtmlNode *node);

/**
 * Returns the decoded text of a text or comment node (HTML entities
 * already resolved, e.g. "&amp;" becomes "&"). Comment delimiters are not
 * included.
 *
 * node  Text or comment node; an element node returns NULL.
 */
const char *eng_html_node_text(const Eng_HtmlNode *node);

/** Returns the number of children of an element node (0 for text/comment
    nodes, and for childless elements). */
int eng_html_node_child_count(const Eng_HtmlNode *node);

/**
 * Returns the child node at index.
 *
 * node   Element node to index into.
 * index  Zero-based child index; must be < eng_html_node_child_count(node).
 * Returns The child node, or NULL if node is not an element or index is
 *         out of range.
 */
Eng_HtmlNode *eng_html_node_child(const Eng_HtmlNode *node, int index);

/** Returns the number of attributes on an element node (0 for
    text/comment nodes, and for elements with no attributes). */
int eng_html_node_attribute_count(const Eng_HtmlNode *node);

/**
 * Returns the attribute at index, as parsed (name lowercased per HTML5
 * rules, value with entities already decoded).
 *
 * node   Element node to index into.
 * index  Zero-based attribute index; must be <
 *        eng_html_node_attribute_count(node).
 * Returns {NULL, NULL} if node is not an element or index is out of range.
 */
Eng_HtmlAttribute eng_html_node_attribute(const Eng_HtmlNode *node, int index);

/**
 * Looks up an attribute by name (case-insensitive per HTML5 rules).
 *
 * node  Element node to search.
 * name  Attribute name, e.g. "href", "class", "id".
 * Returns The attribute's decoded value, or NULL if node is not an
 *         element or has no attribute with that name.
 */
const char *eng_html_node_attribute_value(const Eng_HtmlNode *node,
                                           const char *name);

#ifdef __cplusplus
}
#endif
