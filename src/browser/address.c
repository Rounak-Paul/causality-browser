#include "address.h"

#include <string.h>

/** Tests the ASCII whitespace stripped from either end of address bar input. */
static bool address_space(char byte)
{
    return byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n' ||
           byte == '\v' || byte == '\f';
}

/** Compares the first length bytes of value with a lowercase ASCII prefix. */
static bool address_prefix(const char *value, const char *prefix, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        char byte = value[i];
        if (byte >= 'A' && byte <= 'Z')
            byte = (char)(byte - 'A' + 'a');
        if (byte != prefix[i])
            return false;
    }
    return true;
}

bool browser_address_resolve(const char *input, char *output, size_t capacity)
{
    while (address_space(*input))
        ++input;

    size_t length = strlen(input);
    while (length && address_space(input[length - 1]))
        --length;

    if (!length)
        return false;

    for (size_t i = 0; i < length; ++i) {
        unsigned char byte = (unsigned char)input[i];
        if (byte <= ' ' || byte == 0x7f || byte == '\\')
            return false;
    }

    const char *prefix = "https://";
    size_t scheme_length = 0;
    if (length >= 8 && address_prefix(input, "https://", 8))
        scheme_length = 8;
    else if (length >= 7 && address_prefix(input, "http://", 7))
        scheme_length = 7;

    const char *authority = input + scheme_length;
    size_t authority_length = strcspn(authority, "/?#");
    if (authority_length > length - scheme_length)
        authority_length = length - scheme_length;
    if (!authority_length || memchr(authority, '@', authority_length))
        return false;

    if (!scheme_length && authority[0] != '[') {
        const char *port = memchr(authority, ':', authority_length);
        if (port) {
            const char *end = authority + authority_length;
            if (port == authority || ++port == end)
                return false;
            for (; port < end; ++port) {
                if (*port < '0' || *port > '9')
                    return false;
            }
        }
    }

    size_t prefix_length = scheme_length ? 0 : strlen(prefix);
    if (capacity <= prefix_length || length >= capacity - prefix_length)
        return false;

    memcpy(output, prefix, prefix_length);
    memcpy(output + prefix_length, input, length);
    output[prefix_length + length] = '\0';
    return true;
}
