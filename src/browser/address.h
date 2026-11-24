#pragma once

#include <stdbool.h>
#include <stddef.h>

/** Writes an HTTP(S) address for input to output, which has capacity bytes. */
bool browser_address_resolve(const char *input, char *output, size_t capacity);
