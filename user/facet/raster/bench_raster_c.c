/*
 * Facet Tiger Benchmark — C Reference Implementation
 *
 * Renders a complex tiger-like scene (~30 paths, ~160 cubic Béziers)
 * onto a 512×512 canvas, timed with mach_absolute_time().
 *
 * The tiger is a procedurally-defined scene with overlapping fills,
 * self-intersecting paths, and varying curve densities — the standard
 * stress test for 2D vector engines.
 *
 * Build:  clang -O3 -o bench_raster_c bench_raster_c.c facet_raster.c -lm
 * Run:    ./bench_raster_c
 */

#include "facet_raster.h"
#include <mach/mach_time.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Timing — nanosecond-precision via mach_absolute_time (matches Salt Instant)
 * ═══════════════════════════════════════════════════════════════════════════
 */

static uint64_t time_nanos(void) {
  static mach_timebase_info_data_t info = {0};
  if (info.denom == 0)
    mach_timebase_info(&info);
  uint64_t t = mach_absolute_time();
  return t * info.numer / info.denom;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DCE Prevention — Sum all RGBA values to force the render to be live
 * ═══════════════════════════════════════════════════════════════════════════
 */

static uint64_t pixel_checksum(const FacetCanvas *c) {
  int64_t hash = 0;
  size_t total = (size_t)c->stride * c->height;
  for (size_t i = 0; i < total; i++) {
    int64_t byte_val = (int64_t)c->pixels[i];
    hash = hash ^ byte_val;
    hash = hash * 1099511628211LL;
  }
  return (uint64_t)hash;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tiger Scene — ~30 paths mirroring GhostScript tiger complexity
 *
 * Categories:
 *   - Head outline (1 large path, 8 cubics)
 *   - Ears (2 paths, 4 cubics each)
 *   - Eyes (2 paths, 4 cubics each)
 *   - Pupils (2 small paths)
 *   - Nose (1 path, 4 cubics)
 *   - Mouth (2 paths, 3 cubics each)
 *   - Whiskers (6 quad-curve paths)
 *   - Stripes (8 paths, 3-4 cubics each)
 *   - Inner ear shading (2 semi-transparent paths)
 * ═══════════════════════════════════════════════════════════════════════════
 */

static void build_tiger(FacetCanvas *c) {
  /* Center: (256, 256), scale to fill 512×512 */

  /* ── Head outline ── */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 256.0f, 60.0f);
    facet_path_cubic_to(p, 380.0f, 60.0f, 460.0f, 140.0f, 460.0f, 240.0f);
    facet_path_cubic_to(p, 460.0f, 300.0f, 440.0f, 360.0f, 400.0f, 400.0f);
    facet_path_cubic_to(p, 370.0f, 430.0f, 320.0f, 460.0f, 256.0f, 470.0f);
    facet_path_cubic_to(p, 192.0f, 460.0f, 142.0f, 430.0f, 112.0f, 400.0f);
    facet_path_cubic_to(p, 72.0f, 360.0f, 52.0f, 300.0f, 52.0f, 240.0f);
    facet_path_cubic_to(p, 52.0f, 140.0f, 132.0f, 60.0f, 256.0f, 60.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0xF09030FF); /* orange */
    facet_path_free(p);
  }

  /* ── Left ear ── */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 120.0f, 140.0f);
    facet_path_cubic_to(p, 100.0f, 80.0f, 60.0f, 30.0f, 40.0f, 20.0f);
    facet_path_cubic_to(p, 20.0f, 10.0f, 30.0f, 60.0f, 50.0f, 100.0f);
    facet_path_cubic_to(p, 60.0f, 120.0f, 80.0f, 150.0f, 120.0f, 160.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0xF09030FF);
    facet_path_free(p);
  }

  /* ── Right ear ── */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 392.0f, 140.0f);
    facet_path_cubic_to(p, 412.0f, 80.0f, 452.0f, 30.0f, 472.0f, 20.0f);
    facet_path_cubic_to(p, 492.0f, 10.0f, 482.0f, 60.0f, 462.0f, 100.0f);
    facet_path_cubic_to(p, 452.0f, 120.0f, 432.0f, 150.0f, 392.0f, 160.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0xF09030FF);
    facet_path_free(p);
  }

  /* ── Left inner ear (semi-transparent pink) ── */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 115.0f, 140.0f);
    facet_path_cubic_to(p, 100.0f, 95.0f, 75.0f, 55.0f, 60.0f, 45.0f);
    facet_path_cubic_to(p, 50.0f, 40.0f, 55.0f, 70.0f, 65.0f, 105.0f);
    facet_path_cubic_to(p, 72.0f, 125.0f, 90.0f, 148.0f, 115.0f, 155.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0xFF80A0C0); /* pink, semi-transparent */
    facet_path_free(p);
  }

  /* ── Right inner ear ── */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 397.0f, 140.0f);
    facet_path_cubic_to(p, 412.0f, 95.0f, 437.0f, 55.0f, 452.0f, 45.0f);
    facet_path_cubic_to(p, 462.0f, 40.0f, 457.0f, 70.0f, 447.0f, 105.0f);
    facet_path_cubic_to(p, 440.0f, 125.0f, 422.0f, 148.0f, 397.0f, 155.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0xFF80A0C0);
    facet_path_free(p);
  }

  /* ── Stripes (8 paths, the heart of the benchmark) ── */
  /* Stripe 1: forehead left */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 180.0f, 90.0f);
    facet_path_cubic_to(p, 175.0f, 110.0f, 155.0f, 140.0f, 140.0f, 170.0f);
    facet_path_cubic_to(p, 130.0f, 190.0f, 120.0f, 200.0f, 125.0f, 180.0f);
    facet_path_cubic_to(p, 135.0f, 150.0f, 160.0f, 115.0f, 185.0f, 88.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x302010FF); /* dark brown */
    facet_path_free(p);
  }

  /* Stripe 2: forehead center-left */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 215.0f, 75.0f);
    facet_path_cubic_to(p, 210.0f, 95.0f, 200.0f, 130.0f, 195.0f, 160.0f);
    facet_path_cubic_to(p, 192.0f, 175.0f, 188.0f, 185.0f, 195.0f, 165.0f);
    facet_path_cubic_to(p, 202.0f, 135.0f, 212.0f, 100.0f, 218.0f, 73.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x302010FF);
    facet_path_free(p);
  }

  /* Stripe 3: forehead center-right */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 297.0f, 75.0f);
    facet_path_cubic_to(p, 302.0f, 95.0f, 312.0f, 130.0f, 317.0f, 160.0f);
    facet_path_cubic_to(p, 320.0f, 175.0f, 324.0f, 185.0f, 317.0f, 165.0f);
    facet_path_cubic_to(p, 310.0f, 135.0f, 300.0f, 100.0f, 294.0f, 73.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x302010FF);
    facet_path_free(p);
  }

  /* Stripe 4: forehead right */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 332.0f, 90.0f);
    facet_path_cubic_to(p, 337.0f, 110.0f, 357.0f, 140.0f, 372.0f, 170.0f);
    facet_path_cubic_to(p, 382.0f, 190.0f, 392.0f, 200.0f, 387.0f, 180.0f);
    facet_path_cubic_to(p, 377.0f, 150.0f, 352.0f, 115.0f, 327.0f, 88.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x302010FF);
    facet_path_free(p);
  }

  /* Stripe 5: left cheek */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 100.0f, 220.0f);
    facet_path_cubic_to(p, 85.0f, 250.0f, 80.0f, 290.0f, 90.0f, 320.0f);
    facet_path_cubic_to(p, 95.0f, 335.0f, 100.0f, 325.0f, 95.0f, 295.0f);
    facet_path_cubic_to(p, 90.0f, 265.0f, 95.0f, 240.0f, 105.0f, 218.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x302010FF);
    facet_path_free(p);
  }

  /* Stripe 6: right cheek */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 412.0f, 220.0f);
    facet_path_cubic_to(p, 427.0f, 250.0f, 432.0f, 290.0f, 422.0f, 320.0f);
    facet_path_cubic_to(p, 417.0f, 335.0f, 412.0f, 325.0f, 417.0f, 295.0f);
    facet_path_cubic_to(p, 422.0f, 265.0f, 417.0f, 240.0f, 407.0f, 218.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x302010FF);
    facet_path_free(p);
  }

  /* Stripe 7: left temple */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 145.0f, 100.0f);
    facet_path_cubic_to(p, 130.0f, 120.0f, 110.0f, 160.0f, 100.0f, 200.0f);
    facet_path_cubic_to(p, 96.0f, 215.0f, 105.0f, 210.0f, 108.0f, 195.0f);
    facet_path_cubic_to(p, 118.0f, 155.0f, 135.0f, 118.0f, 150.0f, 98.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x302010FF);
    facet_path_free(p);
  }

  /* Stripe 8: right temple */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 367.0f, 100.0f);
    facet_path_cubic_to(p, 382.0f, 120.0f, 402.0f, 160.0f, 412.0f, 200.0f);
    facet_path_cubic_to(p, 416.0f, 215.0f, 407.0f, 210.0f, 404.0f, 195.0f);
    facet_path_cubic_to(p, 394.0f, 155.0f, 377.0f, 118.0f, 362.0f, 98.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x302010FF);
    facet_path_free(p);
  }

  /* ── Muzzle (white/cream oval) ── */
  {
    FacetPath *p = facet_path_new();
    float cx = 256.0f, cy = 340.0f, rx = 90.0f, ry = 70.0f;
    float kx = 0.5523f * rx, ky = 0.5523f * ry;
    facet_path_move_to(p, cx + rx, cy);
    facet_path_cubic_to(p, cx + rx, cy + ky, cx + kx, cy + ry, cx, cy + ry);
    facet_path_cubic_to(p, cx - kx, cy + ry, cx - rx, cy + ky, cx - rx, cy);
    facet_path_cubic_to(p, cx - rx, cy - ky, cx - kx, cy - ry, cx, cy - ry);
    facet_path_cubic_to(p, cx + kx, cy - ry, cx + rx, cy - ky, cx + rx, cy);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0xFFF0D0FF); /* cream */
    facet_path_free(p);
  }

  /* ── Left eye (white) ── */
  {
    FacetPath *p = facet_path_new();
    float cx = 190.0f, cy = 220.0f, rx = 45.0f, ry = 35.0f;
    float kx = 0.5523f * rx, ky = 0.5523f * ry;
    facet_path_move_to(p, cx + rx, cy);
    facet_path_cubic_to(p, cx + rx, cy + ky, cx + kx, cy + ry, cx, cy + ry);
    facet_path_cubic_to(p, cx - kx, cy + ry, cx - rx, cy + ky, cx - rx, cy);
    facet_path_cubic_to(p, cx - rx, cy - ky, cx - kx, cy - ry, cx, cy - ry);
    facet_path_cubic_to(p, cx + kx, cy - ry, cx + rx, cy - ky, cx + rx, cy);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0xFFFFFFFF);
    facet_path_free(p);
  }

  /* ── Right eye (white) ── */
  {
    FacetPath *p = facet_path_new();
    float cx = 322.0f, cy = 220.0f, rx = 45.0f, ry = 35.0f;
    float kx = 0.5523f * rx, ky = 0.5523f * ry;
    facet_path_move_to(p, cx + rx, cy);
    facet_path_cubic_to(p, cx + rx, cy + ky, cx + kx, cy + ry, cx, cy + ry);
    facet_path_cubic_to(p, cx - kx, cy + ry, cx - rx, cy + ky, cx - rx, cy);
    facet_path_cubic_to(p, cx - rx, cy - ky, cx - kx, cy - ry, cx, cy - ry);
    facet_path_cubic_to(p, cx + kx, cy - ry, cx + rx, cy - ky, cx + rx, cy);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0xFFFFFFFF);
    facet_path_free(p);
  }

  /* ── Left pupil (black) ── */
  {
    FacetPath *p = facet_path_new();
    float cx = 195.0f, cy = 222.0f, r = 18.0f;
    float k = 0.5523f * r;
    facet_path_move_to(p, cx + r, cy);
    facet_path_cubic_to(p, cx + r, cy + k, cx + k, cy + r, cx, cy + r);
    facet_path_cubic_to(p, cx - k, cy + r, cx - r, cy + k, cx - r, cy);
    facet_path_cubic_to(p, cx - r, cy - k, cx - k, cy - r, cx, cy - r);
    facet_path_cubic_to(p, cx + k, cy - r, cx + r, cy - k, cx + r, cy);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x101010FF);
    facet_path_free(p);
  }

  /* ── Right pupil (black) ── */
  {
    FacetPath *p = facet_path_new();
    float cx = 317.0f, cy = 222.0f, r = 18.0f;
    float k = 0.5523f * r;
    facet_path_move_to(p, cx + r, cy);
    facet_path_cubic_to(p, cx + r, cy + k, cx + k, cy + r, cx, cy + r);
    facet_path_cubic_to(p, cx - k, cy + r, cx - r, cy + k, cx - r, cy);
    facet_path_cubic_to(p, cx - r, cy - k, cx - k, cy - r, cx, cy - r);
    facet_path_cubic_to(p, cx + k, cy - r, cx + r, cy - k, cx + r, cy);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x101010FF);
    facet_path_free(p);
  }

  /* ── Nose ── */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 256.0f, 290.0f);
    facet_path_cubic_to(p, 268.0f, 290.0f, 280.0f, 300.0f, 280.0f, 310.0f);
    facet_path_cubic_to(p, 280.0f, 320.0f, 268.0f, 330.0f, 256.0f, 330.0f);
    facet_path_cubic_to(p, 244.0f, 330.0f, 232.0f, 320.0f, 232.0f, 310.0f);
    facet_path_cubic_to(p, 232.0f, 300.0f, 244.0f, 290.0f, 256.0f, 290.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x301818FF);
    facet_path_free(p);
  }

  /* ── Upper mouth line ── */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 256.0f, 330.0f);
    facet_path_line_to(p, 256.0f, 355.0f);
    facet_path_cubic_to(p, 256.0f, 358.0f, 254.0f, 358.0f, 254.0f, 355.0f);
    facet_path_line_to(p, 254.0f, 330.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x301818FF);
    facet_path_free(p);
  }

  /* ── Left mouth curve ── */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 254.0f, 355.0f);
    facet_path_cubic_to(p, 240.0f, 370.0f, 210.0f, 380.0f, 200.0f, 370.0f);
    facet_path_cubic_to(p, 198.0f, 368.0f, 208.0f, 378.0f, 222.0f, 373.0f);
    facet_path_cubic_to(p, 238.0f, 367.0f, 252.0f, 357.0f, 254.0f, 353.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x301818FF);
    facet_path_free(p);
  }

  /* ── Right mouth curve ── */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 258.0f, 355.0f);
    facet_path_cubic_to(p, 272.0f, 370.0f, 302.0f, 380.0f, 312.0f, 370.0f);
    facet_path_cubic_to(p, 314.0f, 368.0f, 304.0f, 378.0f, 290.0f, 373.0f);
    facet_path_cubic_to(p, 274.0f, 367.0f, 260.0f, 357.0f, 258.0f, 353.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x301818FF);
    facet_path_free(p);
  }

  /* ── Whiskers (6 thin quad-curve paths) ── */
  /* Left whisker 1 */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 170.0f, 335.0f);
    facet_path_quad_to(p, 100.0f, 320.0f, 30.0f, 310.0f);
    facet_path_quad_to(p, 100.0f, 325.0f, 170.0f, 338.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x202020FF);
    facet_path_free(p);
  }
  /* Left whisker 2 */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 170.0f, 350.0f);
    facet_path_quad_to(p, 95.0f, 350.0f, 25.0f, 355.0f);
    facet_path_quad_to(p, 95.0f, 354.0f, 170.0f, 353.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x202020FF);
    facet_path_free(p);
  }
  /* Left whisker 3 */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 172.0f, 365.0f);
    facet_path_quad_to(p, 105.0f, 380.0f, 35.0f, 400.0f);
    facet_path_quad_to(p, 105.0f, 384.0f, 172.0f, 368.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x202020FF);
    facet_path_free(p);
  }
  /* Right whisker 1 */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 342.0f, 335.0f);
    facet_path_quad_to(p, 412.0f, 320.0f, 482.0f, 310.0f);
    facet_path_quad_to(p, 412.0f, 325.0f, 342.0f, 338.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x202020FF);
    facet_path_free(p);
  }
  /* Right whisker 2 */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 342.0f, 350.0f);
    facet_path_quad_to(p, 417.0f, 350.0f, 487.0f, 355.0f);
    facet_path_quad_to(p, 417.0f, 354.0f, 342.0f, 353.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x202020FF);
    facet_path_free(p);
  }
  /* Right whisker 3 */
  {
    FacetPath *p = facet_path_new();
    facet_path_move_to(p, 340.0f, 365.0f);
    facet_path_quad_to(p, 407.0f, 380.0f, 477.0f, 400.0f);
    facet_path_quad_to(p, 407.0f, 384.0f, 340.0f, 368.0f);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0x202020FF);
    facet_path_free(p);
  }

  /* ── Eye highlights (small white circles, semi-transparent) ── */
  {
    FacetPath *p = facet_path_new();
    float cx = 200.0f, cy = 216.0f, r = 6.0f;
    float k = 0.5523f * r;
    facet_path_move_to(p, cx + r, cy);
    facet_path_cubic_to(p, cx + r, cy + k, cx + k, cy + r, cx, cy + r);
    facet_path_cubic_to(p, cx - k, cy + r, cx - r, cy + k, cx - r, cy);
    facet_path_cubic_to(p, cx - r, cy - k, cx - k, cy - r, cx, cy - r);
    facet_path_cubic_to(p, cx + k, cy - r, cx + r, cy - k, cx + r, cy);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0xFFFFFFE0);
    facet_path_free(p);
  }
  {
    FacetPath *p = facet_path_new();
    float cx = 322.0f, cy = 216.0f, r = 6.0f;
    float k = 0.5523f * r;
    facet_path_move_to(p, cx + r, cy);
    facet_path_cubic_to(p, cx + r, cy + k, cx + k, cy + r, cx, cy + r);
    facet_path_cubic_to(p, cx - k, cy + r, cx - r, cy + k, cx - r, cy);
    facet_path_cubic_to(p, cx - r, cy - k, cx - k, cy - r, cx, cy - r);
    facet_path_cubic_to(p, cx + k, cy - r, cx + r, cy - k, cx + r, cy);
    facet_path_close(p);
    facet_canvas_fill_nonzero(c, p, 0xFFFFFFE0);
    facet_path_free(p);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main — Benchmark Loop
 * ═══════════════════════════════════════════════════════════════════════════
 */

int main(void) {
  const int WIDTH = 512;
  const int HEIGHT = 512;
  const int WARMUP = 3;
  const int ITERATIONS = 20;

  printf("═══════════════════════════════════════════\n");
  printf("  Facet Tiger Benchmark — C (clang -O3)\n");
  printf("═══════════════════════════════════════════\n");
  printf("Canvas: %dx%d, Iterations: %d\n\n", WIDTH, HEIGHT, ITERATIONS);

  FacetCanvas *c = facet_canvas_new(WIDTH, HEIGHT);
  uint64_t checksum_acc = 0;

  /* Warmup */
  for (int i = 0; i < WARMUP; i++) {
    facet_canvas_clear(c, 0x181820FF);
    build_tiger(c);
  }

  /* Timed iterations */
  uint64_t t_start = time_nanos();

  for (int i = 0; i < ITERATIONS; i++) {
    facet_canvas_clear(c, 0x181820FF);
    build_tiger(c);
    checksum_acc += pixel_checksum(c); /* DCE prevention */
  }

  uint64_t t_end = time_nanos();
  uint64_t elapsed_ns = t_end - t_start;
  uint64_t elapsed_us = elapsed_ns / 1000;
  uint64_t per_frame_us = elapsed_us / (uint64_t)ITERATIONS;

  printf("  Total:      %llu μs (%d frames)\n", (unsigned long long)elapsed_us,
         ITERATIONS);
  printf("  Per frame:  %llu μs\n", (unsigned long long)per_frame_us);
  printf("  Throughput: %.1f frames/sec\n", 1000000.0 / (double)per_frame_us);
  printf("  Checksum:   %llu (DCE guard)\n", (unsigned long long)checksum_acc);

  /* Golden image checksum for single frame */
  facet_canvas_clear(c, 0x181820FF);
  build_tiger(c);
  uint64_t golden = pixel_checksum(c);
  printf("  Golden:     %llu\n", (unsigned long long)golden);

  facet_canvas_free(c);
  return 0;
}
