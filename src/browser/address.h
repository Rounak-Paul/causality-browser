#pragma once

#include <stdbool.h>
#include <stddef.h>

/** Writes an HTTP(S) address for input to output, which has capacity bytes. */
bool browser_address_resolve(const char *input, char *output, size_t capacity);

/** Resolves the typed address or encodes a search query into output. */
bool browser_address_or_search(const char *input, char *output, size_t capacity);
