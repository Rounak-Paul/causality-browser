#include "page_overlay.h"

Ca_Window *browser_page_overlay_create_window(Ca_Instance *instance, const Ca_WindowDesc *desc,
                                      BrowserPageOverlay **overlay, BrowserPageOverlayActionFn action, void *data)
{
    (void)action;
    (void)data;
    *overlay = NULL;
    return ca_window_create(instance, desc);
}

bool browser_page_overlay_install(BrowserPageOverlay *overlay)
{
    (void)overlay;
    return false;
}

bool browser_page_overlay_floats_dock(BrowserPageOverlay *overlay)
{
    (void)overlay;
    return false;
}

void browser_page_overlay_sync_pages(BrowserPageOverlay *overlay)
{
    (void)overlay;
}

void browser_page_overlay_update(BrowserPageOverlay *overlay, const BrowserSurfaceRect *page,
                                 const BrowserSurfaceRect *dock)
{
    (void)overlay;
    (void)page;
    (void)dock;
}

void browser_page_overlay_destroy(BrowserPageOverlay *overlay)
{
    (void)overlay;
}
