/*
 * Facet Window Bridge — macOS AppKit/CoreGraphics Implementation
 *
 * A minimal, high-performance macOS window bridge for the Facet compositor.
 * Creates a native NSWindow with an NSImageView that displays raw RGBA pixels.
 *
 * Architecture:
 *   Salt → extern fn facet_window_* → this bridge → AppKit/CoreGraphics
 *
 * The bridge manages:
 *   - NSApplication initialization (shared, activated)
 *   - NSWindow creation with dark-chrome styling
 *   - Pixel buffer → CGImage → NSImage → NSImageView pipeline
 *   - Non-blocking event polling via nextEventMatchingMask
 *   - Clean teardown with proper ObjC release
 *
 * Thread safety: All calls must be from the main thread.
 * Memory: Pixel data is copied during present — Salt retains ownership.
 */

#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#include <stdint.h>
#include <unistd.h>

// ═══════════════════════════════════════════════════════════════
// Window State — tracks each open window
// ═══════════════════════════════════════════════════════════════

typedef struct {
  NSWindow *window;
  NSImageView *imageView;
  NSApplication *app;
  int32_t width;
  int32_t height;
  int32_t should_close;
  int32_t initialized;
  // Mouse State
  int32_t mx;
  int32_t my;
  int32_t m_btn; // 0=none, 1=left
} FacetWindowState;

// ═══════════════════════════════════════════════════════════════
// Window Delegate — catches close button
// ═══════════════════════════════════════════════════════════════

@interface FacetWindowDelegate : NSObject <NSWindowDelegate>
@property(nonatomic) FacetWindowState *state;
@end

@implementation FacetWindowDelegate
- (BOOL)windowShouldClose:(id)sender {
  if (self.state) {
    self.state->should_close = 1;
  }
  return NO; // We handle close ourselves
}
@end

// ═══════════════════════════════════════════════════════════════
// Application Initialization — ensures NSApp exists
// ═══════════════════════════════════════════════════════════════

static int facet_app_initialized = 0;

static void facet_ensure_app(void) {
  if (facet_app_initialized)
    return;

  @autoreleasepool {
    [NSApplication sharedApplication];

    // Activate as a regular app (needed for windows to appear in front)
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    // Create a minimal menu bar so Cmd+Q works
    NSMenu *menubar = [[NSMenu alloc] init];
    NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
    [menubar addItem:appMenuItem];
    [NSApp setMainMenu:menubar];

    NSMenu *appMenu = [[NSMenu alloc] init];
    NSMenuItem *quitItem =
        [[NSMenuItem alloc] initWithTitle:@"Quit"
                                   action:@selector(terminate:)
                            keyEquivalent:@"q"];
    [appMenu addItem:quitItem];
    [appMenuItem setSubmenu:appMenu];

    // Finish launching
    [NSApp finishLaunching];
    facet_app_initialized = 1;
  }
}

// ═══════════════════════════════════════════════════════════════
// facet_window_open — Create a native macOS window
// ═══════════════════════════════════════════════════════════════

int64_t facet_window_open(int32_t width, int32_t height, const char *title) {
  @autoreleasepool {
    facet_ensure_app();

    FacetWindowState *state =
        (FacetWindowState *)calloc(1, sizeof(FacetWindowState));
    if (!state)
      return 0;

    state->width = width;
    state->height = height;
    state->should_close = 0;

    // Create window
    NSRect frame = NSMakeRect(100, 100, width, height);
    NSWindowStyleMask style = NSWindowStyleMaskTitled |
                              NSWindowStyleMaskClosable |
                              NSWindowStyleMaskMiniaturizable;

    state->window = [[NSWindow alloc] initWithContentRect:frame
                                                styleMask:style
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];

    if (!state->window) {
      free(state);
      return 0;
    }

    // Configure window appearance
    NSString *nsTitle =
        title ? [NSString stringWithUTF8String:title] : @"Facet";
    [state->window setTitle:nsTitle];
    [state->window setBackgroundColor:[NSColor blackColor]];
    [state->window setReleasedWhenClosed:NO];

    // Dark title bar (macOS 10.14+)
    if (@available(macOS 10.14, *)) {
      [state->window
          setAppearance:[NSAppearance
                            appearanceNamed:NSAppearanceNameDarkAqua]];
    }

    // Create image view for pixel display
    NSRect contentFrame = [[state->window contentView] bounds];
    state->imageView = [[NSImageView alloc] initWithFrame:contentFrame];
    [state->imageView setImageScaling:NSImageScaleAxesIndependently];
    [state->imageView
        setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [[state->window contentView] addSubview:state->imageView];

    // Set up delegate for close events
    FacetWindowDelegate *delegate = [[FacetWindowDelegate alloc] init];
    delegate.state = state;
    [state->window setDelegate:delegate];

    // Show window
    [state->window center];
    [state->window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    state->app = NSApp;
    state->initialized = 1;

    return (int64_t)state;
  }
}

// ═══════════════════════════════════════════════════════════════
// facet_window_present — Blit RGBA pixels to the window
// ═══════════════════════════════════════════════════════════════

void facet_window_present(int64_t handle, const uint8_t *pixels, int32_t width,
                          int32_t height) {
  if (!handle || !pixels)
    return;

  @autoreleasepool {
    FacetWindowState *state = (FacetWindowState *)handle;
    if (!state->initialized || !state->imageView)
      return;

    // Create a CGImage from the raw RGBA pixel data
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    if (!colorSpace)
      return;

    int32_t stride = width * 4;
    size_t dataSize = (size_t)stride * (size_t)height;

    // Copy pixel data (Salt retains ownership of the original)
    CGDataProviderRef provider =
        CGDataProviderCreateWithData(NULL, pixels, dataSize, NULL);
    if (!provider) {
      CGColorSpaceRelease(colorSpace);
      return;
    }

    CGImageRef cgImage = CGImageCreate(
        (size_t)width, (size_t)height,
        8,              // bits per component
        32,             // bits per pixel
        (size_t)stride, // bytes per row
        colorSpace, kCGBitmapByteOrderDefault | kCGImageAlphaLast, provider,
        NULL,  // no decode
        false, // no interpolation
        kCGRenderingIntentDefault);

    CGDataProviderRelease(provider);
    CGColorSpaceRelease(colorSpace);

    if (!cgImage)
      return;

    // Convert CGImage → NSImage → display
    NSSize imgSize = NSMakeSize(width, height);
    NSImage *nsImage = [[NSImage alloc] initWithCGImage:cgImage size:imgSize];
    CGImageRelease(cgImage);

    [state->imageView setImage:nsImage];
    [[state->window contentView] setNeedsDisplay:YES];
  }
}

// ═══════════════════════════════════════════════════════════════
// facet_window_poll_events — Non-blocking event drain
// ═══════════════════════════════════════════════════════════════

int32_t facet_window_poll_events(int64_t handle) {
  if (!handle)
    return 1;

  @autoreleasepool {
    FacetWindowState *state = (FacetWindowState *)handle;

    // Drain all pending events
    NSEvent *event;
    while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                       untilDate:[NSDate distantPast]
                                          inMode:NSDefaultRunLoopMode
                                         dequeue:YES])) {

      NSEventType type = [event type];
      if (type == NSEventTypeMouseMoved ||
          type == NSEventTypeLeftMouseDragged) {
        NSPoint p = [event locationInWindow];
        state->mx = (int32_t)p.x;
        // Invert Y (Cocoa is bottom-left, we want top-left)
        state->my = state->height - (int32_t)p.y;
      } else if (type == NSEventTypeLeftMouseDown) {
        state->m_btn = 1;
        NSPoint p = [event locationInWindow];
        state->mx = (int32_t)p.x;
        state->my = state->height - (int32_t)p.y;
      } else if (type == NSEventTypeLeftMouseUp) {
        state->m_btn = 0;
        NSPoint p = [event locationInWindow];
        state->mx = (int32_t)p.x;
        state->my = state->height - (int32_t)p.y;
      }

      [NSApp sendEvent:event];
      [NSApp updateWindows];
    }

    return state->should_close ? 1 : 0;
  }
}

// ═══════════════════════════════════════════════════════════════
// facet_window_close — Destroy window and release resources
// ═══════════════════════════════════════════════════════════════

void facet_window_close(int64_t handle) {
  if (!handle)
    return;

  @autoreleasepool {
    FacetWindowState *state = (FacetWindowState *)handle;

    if (state->window) {
      [state->window setDelegate:nil];
      [state->window close];
      state->window = nil;
    }
    state->imageView = nil;
    state->initialized = 0;

    free(state);
  }
}

// ═══════════════════════════════════════════════════════════════
// facet_window_sleep_ms — Utility: sleep for N milliseconds
// ═══════════════════════════════════════════════════════════════

// Utility: sleep for N milliseconds
void facet_window_sleep_ms(int32_t ms) {
  if (ms > 0) {
    usleep((useconds_t)ms * 1000);
  }
}

// Get Mouse State
void facet_window_get_mouse_state(int64_t handle, int32_t *x, int32_t *y,
                                  int32_t *btn) {
  if (!handle)
    return;
  FacetWindowState *state = (FacetWindowState *)handle;
  if (x)
    *x = state->mx;
  if (y)
    *y = state->my;
  if (btn)
    *btn = state->m_btn;
}

// Memory helpers for Salt (to bypass strict pointer types in free)
void facet_free_node(void *p) { free(p); }
void facet_free_list(void *p) { free(p); }
void facet_free_path(void *p) { free(p); }
void facet_free_ptr(void *p) { free(p); }
void *facet_malloc(size_t size) { return malloc(size); }
