#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#import <WebKit/WebKit.h>

#undef CA_MALLOC
#include "page_overlay.h"

#include <math.h>

enum { DOCK_LANE_HEIGHT = 56 };

/** Places the native page above Causality while exposing the Causality dock. */
@interface BrowserPageLayer : NSView
@property(nonatomic) NSRect page;
@property(nonatomic) CGFloat pageRadius;
@property(nonatomic) NSRect dock;
@property(nonatomic) CGFloat dockRadius;
@property(nonatomic, strong) CAShapeLayer *surfaceMask;
@property(nonatomic, strong) CAShapeLayer *dockMask;
- (BOOL)pointIsInDock:(NSPoint)point;
- (NSView *)pageAtPoint:(NSPoint)point;
- (void)updatePage:(NSRect)page radius:(CGFloat)pageRadius dock:(NSRect)dock dockRadius:(CGFloat)dockRadius;
@end

/** Keeps native pages and the input monitor tied to one Causality window. */
@interface BrowserPageOverlayController : NSObject
@property(nonatomic, weak) NSWindow *window;
@property(nonatomic) BrowserPageOverlayActionFn action;
@property(nonatomic) void *data;
@property(nonatomic, strong) BrowserPageLayer *pageLayer;
@property(nonatomic, strong) id eventMonitor;
@property(nonatomic) BOOL floatingDock;
@end

@implementation BrowserPageOverlayController
@end

/** Tests the same rounded shape used for clipping native content. */
static bool contains_rounded_point(NSRect rect, CGFloat radius, NSPoint point)
{
    if (!NSPointInRect(point, rect))
        return false;
    CGFloat x = point.x - fmin(fmax(point.x, NSMinX(rect) + radius), NSMaxX(rect) - radius);
    CGFloat y = point.y - fmin(fmax(point.y, NSMinY(rect) + radius), NSMaxY(rect) - radius);
    return x * x + y * y <= radius * radius;
}

@implementation BrowserPageLayer

/** Tests a layer-local point against the dock cutout. */
- (BOOL)pointIsInDock:(NSPoint)point
{
    return contains_rounded_point(self.dock, self.dockRadius, point);
}

/** Leaves the shell and dock available to the real Causality widgets. */
- (NSView *)hitTest:(NSPoint)point
{
    NSPoint local = [self convertPoint:point fromView:self.superview];
    if ([self pointIsInDock:local])
        return nil;
    return [self pageAtPoint:point];
}

/** Finds the page under a parent-local point, ignoring the dock cutout. */
- (NSView *)pageAtPoint:(NSPoint)point
{
    NSPoint local = [self convertPoint:point fromView:self.superview];
    if (!contains_rounded_point(self.page, self.pageRadius, local))
        return nil;
    NSView *hit = [super hitTest:point];
    return hit == self ? nil : hit;
}

/** Updates the rounded viewport and floating dock masks without resizing the page. */
- (void)updatePage:(NSRect)page radius:(CGFloat)pageRadius dock:(NSRect)dock dockRadius:(CGFloat)dockRadius
{
    if (NSEqualRects(page, self.page) && pageRadius == self.pageRadius &&
        NSEqualRects(dock, self.dock) && dockRadius == self.dockRadius &&
        NSEqualRects(self.bounds, self.surfaceMask.frame))
        return;

    self.page = page;
    self.pageRadius = pageRadius;
    self.dock = dock;
    self.dockRadius = dockRadius;

    if (!self.surfaceMask)
        self.surfaceMask = [CAShapeLayer layer];
    CGMutablePathRef viewport = CGPathCreateMutable();
    if (!NSIsEmptyRect(page))
        CGPathAddRoundedRect(viewport, NULL, page, pageRadius, pageRadius);

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    self.surfaceMask.frame = self.bounds;
    self.surfaceMask.path = viewport;
    self.layer.mask = self.surfaceMask;
    if (!NSIsEmptyRect(dock)) {
        if (!self.dockMask) {
            self.dockMask = [CAShapeLayer layer];
            self.dockMask.fillRule = kCAFillRuleEvenOdd;
        }
        CGMutablePathRef cutout = CGPathCreateMutable();
        CGPathAddRect(cutout, NULL, self.bounds);
        CGPathAddRoundedRect(cutout, NULL, dock, dockRadius, dockRadius);
        self.dockMask.frame = self.bounds;
        self.dockMask.path = cutout;
        self.surfaceMask.mask = self.dockMask;
        CGPathRelease(cutout);
    } else {
        self.surfaceMask.mask = nil;
    }
    [CATransaction commit];
    CGPathRelease(viewport);
}

@end

/** Converts the opaque C handle to its retained Cocoa controller. */
static BrowserPageOverlayController *controller(BrowserPageOverlay *overlay)
{
    return (__bridge BrowserPageOverlayController *)(void *)overlay;
}

/** Routes browser shortcuts when WebKit owns the window responder. */
static NSEvent *route_key(BrowserPageOverlayController *owner, NSEvent *event)
{
    NSString *key = event.charactersIgnoringModifiers.lowercaseString;
    NSEventModifierFlags flags = event.modifierFlags & (NSEventModifierFlagCommand |
        NSEventModifierFlagOption | NSEventModifierFlagControl | NSEventModifierFlagShift);
    BrowserPageOverlayAction action;
    if (event.keyCode == 53 && flags == 0) {
        action = BROWSER_OVERLAY_ESCAPE;
    } else if (flags == NSEventModifierFlagCommand) {
        if ([key isEqualToString:@"l"]) action = BROWSER_OVERLAY_FOCUS;
        else if ([key isEqualToString:@"r"]) action = BROWSER_OVERLAY_RELOAD;
        else if ([key isEqualToString:@"t"]) action = BROWSER_OVERLAY_NEW_TAB;
        else if ([key isEqualToString:@"w"]) action = BROWSER_OVERLAY_CLOSE_TAB;
        else if ([key isEqualToString:@"["]) action = BROWSER_OVERLAY_BACK;
        else if ([key isEqualToString:@"]"]) action = BROWSER_OVERLAY_FORWARD;
        else return event;
    } else {
        return event;
    }
    return owner.action && owner.action(action, owner.data) ? nil : event;
}

Ca_Window *browser_page_overlay_create_window(Ca_Instance *instance, const Ca_WindowDesc *desc,
                                               BrowserPageOverlay **overlay, BrowserPageOverlayActionFn action,
                                               void *data)
{
    *overlay = NULL;
    NSSet<NSWindow *> *previous = [NSSet setWithArray:NSApp.windows];
    Ca_Window *window = ca_window_create(instance, desc);
    if (!window)
        return NULL;

    NSWindow *created = nil;
    for (NSWindow *candidate in NSApp.windows) {
        if (!candidate.visible || [previous containsObject:candidate])
            continue;
        if (created)
            return window;
        created = candidate;
    }
    if (!created.contentView.superview)
        return window;

    BrowserPageOverlayController *owner = [BrowserPageOverlayController new];
    owner.window = created;
    owner.action = action;
    owner.data = data;
    *overlay = (__bridge_retained BrowserPageOverlay *)owner;
    return window;
}

bool browser_page_overlay_install(BrowserPageOverlay *overlay)
{
    if (!overlay)
        return false;
    BrowserPageOverlayController *owner = controller(overlay);
    NSView *content = owner.window.contentView;
    NSView *parent = content.superview;
    if (!parent)
        return false;

    NSRect frame = [parent convertRect:content.bounds fromView:content];
    owner.pageLayer = [[BrowserPageLayer alloc] initWithFrame:frame];
    owner.pageLayer.wantsLayer = YES;
    owner.pageLayer.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [parent addSubview:owner.pageLayer positioned:NSWindowAbove relativeTo:content];
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
    if (@available(macOS 26.0, *))
        owner.floatingDock = YES;
#endif

    __weak BrowserPageOverlayController *weakOwner = owner;
    NSEventMask events = NSEventMaskKeyDown | NSEventMaskLeftMouseDown | NSEventMaskScrollWheel;
    owner.eventMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:events handler:^NSEvent *(NSEvent *event) {
        BrowserPageOverlayController *binding = weakOwner;
        if (!binding || event.window != binding.window)
            return event;
        if (event.type == NSEventTypeKeyDown)
            return route_key(binding, event);
        BrowserPageLayer *layer = binding.pageLayer;
        if ([layer pointIsInDock:[layer convertPoint:event.locationInWindow fromView:nil]]) {
            if (event.type == NSEventTypeLeftMouseDown) {
                [binding.window makeFirstResponder:binding.window.contentView];
            } else {
                NSPoint point = [layer.superview convertPoint:event.locationInWindow fromView:nil];
                NSView *page = [layer pageAtPoint:point];
                if (page) {
                    [page scrollWheel:event];
                    return nil;
                }
            }
        }
        return event;
    }];
    return true;
}

bool browser_page_overlay_floats_dock(BrowserPageOverlay *overlay)
{
    return overlay && controller(overlay).floatingDock;
}

void browser_page_overlay_sync_pages(BrowserPageOverlay *overlay)
{
    if (!overlay)
        return;
    BrowserPageOverlayController *owner = controller(overlay);
    NSView *content = owner.window.contentView;
    for (NSView *clip in content.superview.subviews) {
        if (clip == content || clip == owner.pageLayer)
            continue;
        for (NSView *child in clip.subviews) {
            if ([child isKindOfClass:WKWebView.class]) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
                if (@available(macOS 26.0, *)) {
                    WKWebView *page = (WKWebView *)child;
                    page.obscuredContentInsets = NSEdgeInsetsMake(0, 0, owner.floatingDock ? DOCK_LANE_HEIGHT : 0, 0);
                }
#endif
                [owner.pageLayer addSubview:clip];
                break;
            }
        }
    }
}

/** Converts a Causality box to the native page layer's coordinates. */
static NSRect layer_rect(BrowserPageLayer *layer, NSView *content, const BrowserSurfaceRect *surface)
{
    if (!surface)
        return NSZeroRect;
    NSRect rect = NSMakeRect(surface->x, surface->y, surface->width, surface->height);
    if (!content.isFlipped)
        rect.origin.y = NSHeight(content.bounds) - NSMaxY(rect);
    return [layer convertRect:rect fromView:content];
}

void browser_page_overlay_update(BrowserPageOverlay *overlay, const BrowserSurfaceRect *page,
                                 const BrowserSurfaceRect *dock)
{
    if (!overlay)
        return;
    @autoreleasepool {
        BrowserPageOverlayController *owner = controller(overlay);
        NSView *content = owner.window.contentView;
        if (!content)
            return;
        BrowserPageLayer *layer = owner.pageLayer;
        NSRect viewport = layer_rect(layer, content, page);
        NSRect cutout = layer_rect(layer, content, dock);
        CGFloat pageRadius = page ? fmin(page->radius, fmin(NSWidth(viewport), NSHeight(viewport)) * 0.5) : 0;
        CGFloat dockRadius = dock ? fmin(dock->radius, fmin(NSWidth(cutout), NSHeight(cutout)) * 0.5) : 0;
        [layer updatePage:viewport radius:pageRadius dock:cutout dockRadius:dockRadius];
    }
}

void browser_page_overlay_destroy(BrowserPageOverlay *overlay)
{
    if (!overlay)
        return;
    BrowserPageOverlayController *owner = (__bridge_transfer BrowserPageOverlayController *)overlay;
    if (owner.eventMonitor)
        [NSEvent removeMonitor:owner.eventMonitor];
    [owner.pageLayer removeFromSuperview];
    owner.action = NULL;
}
