#pragma once

#include <causality.h>

typedef struct BrowserPageOverlay BrowserPageOverlay;

/** Identifies a browser shortcut intercepted while the WebView has focus. */
typedef enum BrowserPageOverlayAction {
    BROWSER_OVERLAY_FOCUS,
    BROWSER_OVERLAY_BACK,
    BROWSER_OVERLAY_FORWARD,
    BROWSER_OVERLAY_RELOAD,
    BROWSER_OVERLAY_ESCAPE,
    BROWSER_OVERLAY_NEW_TAB,
    BROWSER_OVERLAY_CLOSE_TAB,
} BrowserPageOverlayAction;

typedef bool (*BrowserPageOverlayActionFn)(BrowserPageOverlayAction action, void *data);

/** Creates the Causality window and binds its native page layer when the platform supports it. */
Ca_Window *browser_page_overlay_create_window(Ca_Instance *instance, const Ca_WindowDesc *desc,
                                      BrowserPageOverlay **overlay, BrowserPageOverlayActionFn action, void *data);

/** Installs the rounded native page layer above the Causality canvas. */
bool browser_page_overlay_install(BrowserPageOverlay *overlay);

/** Reports whether the native WebView supports a floating dock without obscuring fixed page controls. */
bool browser_page_overlay_floats_dock(BrowserPageOverlay *overlay);

/** Fits newly created native pages into the rounded native page layer. */
void browser_page_overlay_sync_pages(BrowserPageOverlay *overlay);

/** Describes a rounded Causality surface in window-local logical points. */
typedef struct BrowserSurfaceRect {
    float x, y, width, height, radius;
} BrowserSurfaceRect;

/** Updates the rounded page aperture and the cutout for the floating dock. */
void browser_page_overlay_update(BrowserPageOverlay *overlay, const BrowserSurfaceRect *page,
                                 const BrowserSurfaceRect *dock);

/** Releases the platform binding and removes the native layer and event monitor. */
void browser_page_overlay_destroy(BrowserPageOverlay *overlay);
