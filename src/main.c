// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rounak Paul.

#include <causality.h>
#include <stdio.h>

/**
 * Application entry point.
 *
 * Initializes a Causality instance and opens the main browser window,
 * then runs the event loop until the window is closed.
 *
 * Returns 0 on clean shutdown, 1 if instance or window creation fails.
 */
int main(void)
{
    Ca_InstanceDesc instance_desc = {
        .app_name             = "Causality Browser",
        .prefer_dedicated_gpu = true,
    };
    Ca_Instance *instance = ca_instance_create(&instance_desc);
    if (!instance) {
        fprintf(stderr, "Failed to create causality instance\n");
        return 1;
    }

    Ca_WindowDesc window_desc = {
        .title  = "Causality Browser",
        .width  = 1280,
        .height = 800,
    };
    Ca_Window *window = ca_window_create(instance, &window_desc);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        ca_instance_destroy(instance);
        return 1;
    }

    while (ca_instance_tick(instance)) { }

    ca_instance_destroy(instance);
    return 0;
}
