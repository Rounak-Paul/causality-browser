#pragma once

#include <causality.h>

/** Identifies a built-in palette. Numeric values are used in saved preferences. */
typedef enum BrowserTheme {
    BROWSER_THEME_DARK = 0,
    BROWSER_THEME_LIGHT = 1,
    BROWSER_THEME_DUSK = 2,
    BROWSER_THEME_COUNT
} BrowserTheme;

/** Creates a Causality instance with the platform's proportional UI fonts when available. */
Ca_Instance *browser_create_ui_instance(const char *app_name);

/** Returns the display name for a supported theme. */
const char *browser_theme_name(BrowserTheme theme);

/** Creates a caller-owned stylesheet for a supported theme and writes its canvas color. */
Ca_Stylesheet *browser_theme_create(BrowserTheme theme, uint32_t *canvas_color);

/** Reads the user's theme preference, with Dark as the default. */
BrowserTheme browser_theme_load(void);

/** Saves theme as the user's default for future launches. */
bool browser_theme_save(BrowserTheme theme);
