#include "browser/window.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Starts the browser with an optional initial address in argv[1]. */
int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        puts("Causality Browser " BROWSER_VERSION);
        return EXIT_SUCCESS;
    }
    if (argc > 2 || (argc == 2 && argv[1][0] == '-')) {
        fprintf(stderr, "Usage: %s [address | --version]\n", argv[0]);
        return EXIT_FAILURE;
    }
    return browser_run(argc == 2 ? argv[1] : NULL);
}
