/*
 * Facet Window Bridge — macOS AppKit/CoreGraphics
 *
 * Provides C-callable functions for Salt to create a native macOS window
 * and blit RGBA pixel buffers to it. Uses AppKit (NSWindow, NSImageView)
 * with CoreGraphics for pixel buffer rendering.
 *
 * Compile: clang -ObjC -framework Cocoa -framework CoreGraphics
 */

#ifndef FACET_WINDOW_H
#define FACET_WINDOW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Open a native macOS window with the given dimensions.
 * Returns an opaque handle (cast to i64 in Salt).
 * title: null-terminated C string for the window title.
 * Returns 0 on failure.
 */
int64_t facet_window_open(int32_t width, int32_t height, const char *title);

/*
 * Blit an RGBA pixel buffer to the window.
 * pixels: pointer to width*height*4 bytes of RGBA data.
 * The pixel data is copied — the caller retains ownership.
 */
void facet_window_present(int64_t handle, const uint8_t *pixels, int32_t width,
                          int32_t height);

/*
 * Poll for window events (non-blocking).
 * Returns 1 if the window should close, 0 otherwise.
 */
int32_t facet_window_poll_events(int64_t handle);

/*
 * Close and destroy the window, releasing all resources.
 */
void facet_window_close(int64_t handle);

/*
 * Sleep for the given number of milliseconds.
 * Utility for frame timing in animation loops.
 */
void facet_window_sleep_ms(int32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* FACET_WINDOW_H */
