/*
 * Facet Raster — TDD Test Suite
 *
 * Tests are written BEFORE the implementation. Each test validates a specific
 * property of the rasterizer with deterministic, pixel-level assertions.
 *
 * Build & Run:
 *   clang -O2 -Wall -Wextra -o test_raster test_raster.c facet_raster.c -lm
 *   ./test_raster
 */

#include "facet_raster.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Test Framework (minimal, no dependencies)
 * ═══════════════════════════════════════════════════════════════════════════
 */

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name) static void name(void)
#define RUN_TEST(name)                                                         \
  do {                                                                         \
    g_tests_run++;                                                             \
    printf("  %-50s ", #name);                                                 \
    fflush(stdout);                                                            \
    name();                                                                    \
    g_tests_passed++;                                                          \
    printf("✓\n");                                                             \
  } while (0)

#define ASSERT(cond)                                                           \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("✗\n    FAIL: %s (line %d)\n", #cond, __LINE__);                  \
      g_tests_failed++;                                                        \
      g_tests_passed--; /* undo the pre-increment in RUN_TEST */               \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ASSERT_EQ(a, b)                                                        \
  do {                                                                         \
    if ((a) != (b)) {                                                          \
      printf("✗\n    FAIL: %s == %s (%d != %d) (line %d)\n", #a, #b, (int)(a), \
             (int)(b), __LINE__);                                              \
      g_tests_failed++;                                                        \
      g_tests_passed--;                                                        \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ASSERT_NEAR(a, b, eps)                                                 \
  do {                                                                         \
    if (fabs((double)(a) - (double)(b)) > (double)(eps)) {                     \
      printf("✗\n    FAIL: |%s - %s| > %s (%.4f vs %.4f) (line %d)\n", #a, #b, \
             #eps, (double)(a), (double)(b), __LINE__);                        \
      g_tests_failed++;                                                        \
      g_tests_passed--;                                                        \
      return;                                                                  \
    }                                                                          \
  } while (0)

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper: Count non-background pixels on a canvas
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* Count pixels with alpha > 0 */
static uint32_t count_visible_pixels(const FacetCanvas *c) {
  uint32_t count = 0;
  for (uint32_t y = 0; y < c->height; y++) {
    const uint8_t *row = c->pixels + y * c->stride;
    for (uint32_t x = 0; x < c->width; x++) {
      if (row[x * 4 + 3] > 0)
        count++;
    }
  }
  return count;
}

/* Get pixel RGBA at (x, y) as packed uint32 */
static uint32_t get_pixel(const FacetCanvas *c, uint32_t x, uint32_t y) {
  if (x >= c->width || y >= c->height)
    return 0;
  const uint8_t *p = c->pixels + y * c->stride + x * 4;
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* Check that pixel at (x, y) has alpha > 0 */
static int pixel_visible(const FacetCanvas *c, uint32_t x, uint32_t y) {
  if (x >= c->width || y >= c->height)
    return 0;
  return c->pixels[y * c->stride + x * 4 + 3] > 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Path Construction Tests
 * ═══════════════════════════════════════════════════════════════════════════
 */

TEST(test_path_create_free) {
  FacetPath *p = facet_path_new();
  ASSERT(p != NULL);
  ASSERT_EQ(p->count, 0);
  facet_path_free(p);
}

TEST(test_path_move_and_line) {
  FacetPath *p = facet_path_new();
  facet_path_move_to(p, 10.0f, 20.0f);
  facet_path_line_to(p, 30.0f, 40.0f);
  ASSERT_EQ(p->count, 2);
  ASSERT_EQ(p->cmds[0].type, FACET_CMD_MOVE_TO);
  ASSERT_NEAR(p->cmds[0].points[0].x, 10.0f, 0.001f);
  ASSERT_NEAR(p->cmds[0].points[0].y, 20.0f, 0.001f);
  ASSERT_EQ(p->cmds[1].type, FACET_CMD_LINE_TO);
  ASSERT_NEAR(p->cmds[1].points[0].x, 30.0f, 0.001f);
  facet_path_free(p);
}

TEST(test_path_quad) {
  FacetPath *p = facet_path_new();
  facet_path_move_to(p, 0.0f, 0.0f);
  facet_path_quad_to(p, 50.0f, 100.0f, 100.0f, 0.0f);
  ASSERT_EQ(p->count, 2);
  ASSERT_EQ(p->cmds[1].type, FACET_CMD_QUAD_TO);
  ASSERT_NEAR(p->cmds[1].points[0].x, 50.0f, 0.001f);  /* control */
  ASSERT_NEAR(p->cmds[1].points[1].x, 100.0f, 0.001f); /* end */
  facet_path_free(p);
}

TEST(test_path_cubic) {
  FacetPath *p = facet_path_new();
  facet_path_move_to(p, 0.0f, 0.0f);
  facet_path_cubic_to(p, 33.0f, 100.0f, 66.0f, 100.0f, 100.0f, 0.0f);
  ASSERT_EQ(p->count, 2);
  ASSERT_EQ(p->cmds[1].type, FACET_CMD_CUBIC_TO);
  facet_path_free(p);
}

TEST(test_path_close) {
  FacetPath *p = facet_path_new();
  facet_path_move_to(p, 0.0f, 0.0f);
  facet_path_line_to(p, 100.0f, 0.0f);
  facet_path_line_to(p, 100.0f, 100.0f);
  facet_path_close(p);
  ASSERT_EQ(p->count, 4);
  ASSERT_EQ(p->cmds[3].type, FACET_CMD_CLOSE);
  facet_path_free(p);
}

TEST(test_path_capacity_growth) {
  FacetPath *p = facet_path_new();
  facet_path_move_to(p, 0.0f, 0.0f);
  /* Add many line segments to test dynamic array growth */
  for (int i = 0; i < 1000; i++) {
    facet_path_line_to(p, (float)i, (float)(i * 2));
  }
  ASSERT_EQ(p->count, 1001);
  ASSERT_NEAR(p->cmds[500].points[0].x, 499.0f, 0.001f);
  facet_path_free(p);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Canvas Tests
 * ═══════════════════════════════════════════════════════════════════════════
 */

TEST(test_canvas_create) {
  FacetCanvas *c = facet_canvas_new(100, 100);
  ASSERT(c != NULL);
  ASSERT_EQ(c->width, 100);
  ASSERT_EQ(c->height, 100);
  ASSERT(c->stride >= 400); /* at least width * 4 */
  ASSERT(c->pixels != NULL);
  facet_canvas_free(c);
}

TEST(test_canvas_clear) {
  FacetCanvas *c = facet_canvas_new(10, 10);
  facet_canvas_clear(c, 0xFF0000FF); /* red, full alpha */
  /* Check first pixel */
  uint32_t px = get_pixel(c, 0, 0);
  ASSERT_EQ(px, 0xFF0000FF);
  /* Check last pixel */
  px = get_pixel(c, 9, 9);
  ASSERT_EQ(px, 0xFF0000FF);
  facet_canvas_free(c);
}

TEST(test_canvas_clear_transparent) {
  FacetCanvas *c = facet_canvas_new(10, 10);
  facet_canvas_clear(c, 0x00000000);
  uint32_t visible = count_visible_pixels(c);
  ASSERT_EQ(visible, 0);
  facet_canvas_free(c);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Bézier Flattening Tests
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  FacetPoint *points;
  size_t count;
  size_t capacity;
} PointCollector;

static void collect_point(float x, float y, void *user_data) {
  PointCollector *pc = (PointCollector *)user_data;
  if (pc->count >= pc->capacity) {
    pc->capacity = pc->capacity ? pc->capacity * 2 : 64;
    pc->points = realloc(pc->points, pc->capacity * sizeof(FacetPoint));
  }
  pc->points[pc->count].x = x;
  pc->points[pc->count].y = y;
  pc->count++;
}

TEST(test_flatten_quad_degenerate) {
  /* Control point on the line → should flatten to ~2 points (start, end) */
  FacetPoint p0 = {0.0f, 0.0f};
  FacetPoint p1 = {50.0f, 0.0f}; /* on the line */
  FacetPoint p2 = {100.0f, 0.0f};

  PointCollector pc = {0};
  facet_flatten_quad(p0, p1, p2, 0.25f, collect_point, &pc);

  /* Degenerate curve should produce very few points */
  ASSERT(pc.count >= 1);
  ASSERT(pc.count <= 4);
  /* Last point should be the endpoint */
  ASSERT_NEAR(pc.points[pc.count - 1].x, 100.0f, 0.5f);
  ASSERT_NEAR(pc.points[pc.count - 1].y, 0.0f, 0.5f);
  free(pc.points);
}

TEST(test_flatten_quad_arc) {
  /* Quarter-circle-like arc */
  FacetPoint p0 = {0.0f, 0.0f};
  FacetPoint p1 = {0.0f, 100.0f};
  FacetPoint p2 = {100.0f, 100.0f};

  PointCollector pc = {0};
  facet_flatten_quad(p0, p1, p2, 0.25f, collect_point, &pc);

  /* Arc should produce multiple points */
  ASSERT(pc.count >= 4);
  /* All points should be in the bounding box [0, 100] × [0, 100] */
  for (size_t i = 0; i < pc.count; i++) {
    ASSERT(pc.points[i].x >= -0.1f && pc.points[i].x <= 100.1f);
    ASSERT(pc.points[i].y >= -0.1f && pc.points[i].y <= 100.1f);
  }
  /* Last point should be the endpoint */
  ASSERT_NEAR(pc.points[pc.count - 1].x, 100.0f, 0.5f);
  ASSERT_NEAR(pc.points[pc.count - 1].y, 100.0f, 0.5f);
  free(pc.points);
}

TEST(test_flatten_cubic) {
  /* S-curve */
  FacetPoint p0 = {0.0f, 0.0f};
  FacetPoint p1 = {33.0f, 100.0f};
  FacetPoint p2 = {66.0f, -100.0f};
  FacetPoint p3 = {100.0f, 0.0f};

  PointCollector pc = {0};
  facet_flatten_cubic(p0, p1, p2, p3, 0.25f, collect_point, &pc);

  /* S-curve should produce many points */
  ASSERT(pc.count >= 8);
  /* Last point should be the endpoint */
  ASSERT_NEAR(pc.points[pc.count - 1].x, 100.0f, 0.5f);
  ASSERT_NEAR(pc.points[pc.count - 1].y, 0.0f, 0.5f);
  free(pc.points);
}

TEST(test_flatten_tolerance_coarse) {
  /* Same curve, coarser tolerance → fewer points */
  FacetPoint p0 = {0.0f, 0.0f};
  FacetPoint p1 = {0.0f, 100.0f};
  FacetPoint p2 = {100.0f, 100.0f};

  PointCollector fine = {0};
  facet_flatten_quad(p0, p1, p2, 0.1f, collect_point, &fine);

  PointCollector coarse = {0};
  facet_flatten_quad(p0, p1, p2, 5.0f, collect_point, &coarse);

  /* Coarser tolerance should produce fewer points */
  ASSERT(coarse.count < fine.count);
  free(fine.points);
  free(coarse.points);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Fill Tests
 * ═══════════════════════════════════════════════════════════════════════════
 */

TEST(test_fill_triangle) {
  /* Triangle: (50,10) → (90,90) → (10,90) → close
   * Area = 0.5 * base * height = 0.5 * 80 * 80 = 3200 px */
  FacetCanvas *c = facet_canvas_new(100, 100);
  facet_canvas_clear(c, 0x00000000);

  FacetPath *p = facet_path_new();
  facet_path_move_to(p, 50.0f, 10.0f);
  facet_path_line_to(p, 90.0f, 90.0f);
  facet_path_line_to(p, 10.0f, 90.0f);
  facet_path_close(p);

  facet_canvas_fill_nonzero(c, p, 0xFF0000FF);

  uint32_t visible = count_visible_pixels(c);
  /* Allow ±10% for anti-aliasing edge pixels */
  ASSERT(visible > 2800);
  ASSERT(visible < 3600);

  /* Interior pixel should be fully opaque red */
  uint32_t interior = get_pixel(c, 50, 50);
  ASSERT_EQ(interior, 0xFF0000FF);

  /* Exterior pixel should be transparent */
  ASSERT(!pixel_visible(c, 0, 0));
  ASSERT(!pixel_visible(c, 99, 0));

  facet_path_free(p);
  facet_canvas_free(c);
}

TEST(test_fill_rectangle) {
  /* Rectangle: 20x20 at (10, 10) → area = 400 px */
  FacetCanvas *c = facet_canvas_new(50, 50);
  facet_canvas_clear(c, 0x00000000);

  FacetPath *p = facet_path_new();
  facet_path_move_to(p, 10.0f, 10.0f);
  facet_path_line_to(p, 30.0f, 10.0f);
  facet_path_line_to(p, 30.0f, 30.0f);
  facet_path_line_to(p, 10.0f, 30.0f);
  facet_path_close(p);

  facet_canvas_fill_nonzero(c, p, 0x00FF00FF);

  uint32_t visible = count_visible_pixels(c);
  /* Rectangle should produce ~400 px, very tight tolerance (~1% for edge AA) */
  ASSERT(visible >= 380);
  ASSERT(visible <= 440);

  /* Interior fully opaque green */
  uint32_t px = get_pixel(c, 20, 20);
  ASSERT_EQ(px, 0x00FF00FF);

  /* Outside the rect */
  ASSERT(!pixel_visible(c, 5, 5));
  ASSERT(!pixel_visible(c, 35, 35));

  facet_path_free(p);
  facet_canvas_free(c);
}

TEST(test_fill_circle_approx) {
  /* Approximate circle with 4 cubic Béziers (standard SVG approach)
   * Radius = 40, center = (50, 50)
   * Area = π × 40² ≈ 5027 px
   * kappa = 4 * (√2 - 1) / 3 ≈ 0.5523 */
  float r = 40.0f;
  float cx = 50.0f, cy = 50.0f;
  float k = 0.5523f * r;

  FacetCanvas *c = facet_canvas_new(100, 100);
  facet_canvas_clear(c, 0x00000000);

  FacetPath *p = facet_path_new();
  facet_path_move_to(p, cx + r, cy);
  facet_path_cubic_to(p, cx + r, cy + k, cx + k, cy + r, cx, cy + r);
  facet_path_cubic_to(p, cx - k, cy + r, cx - r, cy + k, cx - r, cy);
  facet_path_cubic_to(p, cx - r, cy - k, cx - k, cy - r, cx, cy - r);
  facet_path_cubic_to(p, cx + k, cy - r, cx + r, cy - k, cx + r, cy);
  facet_path_close(p);

  facet_canvas_fill_nonzero(c, p, 0x0000FFFF);

  uint32_t visible = count_visible_pixels(c);
  float expected = 3.14159f * r * r;
  /* Allow ±5% for cubic approximation + AA */
  ASSERT(visible > (uint32_t)(expected * 0.95f));
  ASSERT(visible < (uint32_t)(expected * 1.05f));

  /* Center should be fully opaque blue */
  uint32_t px = get_pixel(c, 50, 50);
  ASSERT_EQ(px, 0x0000FFFF);

  /* Corners should be transparent */
  ASSERT(!pixel_visible(c, 0, 0));
  ASSERT(!pixel_visible(c, 99, 0));

  facet_path_free(p);
  facet_canvas_free(c);
}

TEST(test_fill_winding_nonzero) {
  /* Two concentric rectangles, same winding direction.
   * Non-zero rule: inner rectangle does NOT create a hole. */
  FacetCanvas *c = facet_canvas_new(100, 100);
  facet_canvas_clear(c, 0x00000000);

  FacetPath *p = facet_path_new();
  /* Outer rectangle (CW): (10,10) → (90,10) → (90,90) → (10,90) */
  facet_path_move_to(p, 10.0f, 10.0f);
  facet_path_line_to(p, 90.0f, 10.0f);
  facet_path_line_to(p, 90.0f, 90.0f);
  facet_path_line_to(p, 10.0f, 90.0f);
  facet_path_close(p);
  /* Inner rectangle (same CW): (30,30) → (70,30) → (70,70) → (30,70) */
  facet_path_move_to(p, 30.0f, 30.0f);
  facet_path_line_to(p, 70.0f, 30.0f);
  facet_path_line_to(p, 70.0f, 70.0f);
  facet_path_line_to(p, 30.0f, 70.0f);
  facet_path_close(p);

  facet_canvas_fill(c, p, 0xFFFFFFFF, FACET_WINDING_NONZERO);

  /* Center should be filled (both contribute, winding count = 2, nonzero →
   * fill) */
  ASSERT(pixel_visible(c, 50, 50));

  facet_path_free(p);
  facet_canvas_free(c);
}

TEST(test_fill_winding_evenodd) {
  /* Two concentric rectangles, same winding direction.
   * Even-odd rule: inner rectangle DOES create a hole. */
  FacetCanvas *c = facet_canvas_new(100, 100);
  facet_canvas_clear(c, 0x00000000);

  FacetPath *p = facet_path_new();
  /* Outer rectangle */
  facet_path_move_to(p, 10.0f, 10.0f);
  facet_path_line_to(p, 90.0f, 10.0f);
  facet_path_line_to(p, 90.0f, 90.0f);
  facet_path_line_to(p, 10.0f, 90.0f);
  facet_path_close(p);
  /* Inner rectangle (same direction) */
  facet_path_move_to(p, 30.0f, 30.0f);
  facet_path_line_to(p, 70.0f, 30.0f);
  facet_path_line_to(p, 70.0f, 70.0f);
  facet_path_line_to(p, 30.0f, 70.0f);
  facet_path_close(p);

  facet_canvas_fill(c, p, 0xFFFFFFFF, FACET_WINDING_EVENODD);

  /* Center should NOT be filled (even-odd: 2 crossings = even = not filled) */
  ASSERT(!pixel_visible(c, 50, 50));
  /* Between outer and inner should be filled */
  ASSERT(pixel_visible(c, 15, 15));

  facet_path_free(p);
  facet_canvas_free(c);
}

TEST(test_canvas_bounds_safety) {
  /* Fill a path that extends outside the canvas.
   * No pixel writes should occur outside canvas bounds. */
  FacetCanvas *c = facet_canvas_new(50, 50);
  facet_canvas_clear(c, 0x00000000);

  /* Remember the allocated pixel buffer size */
  size_t buf_size = c->stride * c->height;

  FacetPath *p = facet_path_new();
  facet_path_move_to(p, -50.0f, -50.0f);
  facet_path_line_to(p, 200.0f, -50.0f);
  facet_path_line_to(p, 200.0f, 200.0f);
  facet_path_line_to(p, -50.0f, 200.0f);
  facet_path_close(p);

  facet_canvas_fill_nonzero(c, p, 0xFF0000FF);

  /* All pixels in canvas should be filled (the rect covers the entire canvas)
   */
  uint32_t visible = count_visible_pixels(c);
  ASSERT_EQ(visible, 50 * 50);

  /* Verify no memory corruption: we didn't segfault (reaching here is success)
   */
  (void)buf_size;

  facet_path_free(p);
  facet_canvas_free(c);
}

TEST(test_fill_empty_path) {
  /* Empty path should not crash and should produce no pixels */
  FacetCanvas *c = facet_canvas_new(50, 50);
  facet_canvas_clear(c, 0x00000000);

  FacetPath *p = facet_path_new();
  facet_canvas_fill_nonzero(c, p, 0xFF0000FF);

  uint32_t visible = count_visible_pixels(c);
  ASSERT_EQ(visible, 0);

  facet_path_free(p);
  facet_canvas_free(c);
}

TEST(test_fill_single_point) {
  /* Path with only a move_to should produce no pixels */
  FacetCanvas *c = facet_canvas_new(50, 50);
  facet_canvas_clear(c, 0x00000000);

  FacetPath *p = facet_path_new();
  facet_path_move_to(p, 25.0f, 25.0f);
  facet_canvas_fill_nonzero(c, p, 0xFF0000FF);

  uint32_t visible = count_visible_pixels(c);
  ASSERT_EQ(visible, 0);

  facet_path_free(p);
  facet_canvas_free(c);
}

TEST(test_fill_multiple_subpaths) {
  /* Two separate triangles in the same path (via two move_to's) */
  FacetCanvas *c = facet_canvas_new(100, 50);
  facet_canvas_clear(c, 0x00000000);

  FacetPath *p = facet_path_new();
  /* Triangle 1: left side */
  facet_path_move_to(p, 10.0f, 10.0f);
  facet_path_line_to(p, 40.0f, 10.0f);
  facet_path_line_to(p, 25.0f, 40.0f);
  facet_path_close(p);
  /* Triangle 2: right side */
  facet_path_move_to(p, 60.0f, 10.0f);
  facet_path_line_to(p, 90.0f, 10.0f);
  facet_path_line_to(p, 75.0f, 40.0f);
  facet_path_close(p);

  facet_canvas_fill_nonzero(c, p, 0xFFFF00FF);

  /* Both triangles should be visible */
  ASSERT(pixel_visible(c, 25, 25));
  ASSERT(pixel_visible(c, 75, 25));
  /* Gap between them should be empty */
  ASSERT(!pixel_visible(c, 50, 25));

  facet_path_free(p);
  facet_canvas_free(c);
}

TEST(test_fill_with_curves) {
  /* Fill a shape with a quadratic curve edge */
  FacetCanvas *c = facet_canvas_new(100, 100);
  facet_canvas_clear(c, 0x00000000);

  FacetPath *p = facet_path_new();
  facet_path_move_to(p, 10.0f, 50.0f);
  facet_path_quad_to(p, 50.0f, 10.0f, 90.0f, 50.0f); /* curved top */
  facet_path_line_to(p, 90.0f, 90.0f);
  facet_path_line_to(p, 10.0f, 90.0f);
  facet_path_close(p);

  facet_canvas_fill_nonzero(c, p, 0xFF00FFFF);

  /* Bottom center should be filled */
  ASSERT(pixel_visible(c, 50, 80));
  /* Top center (above curve) should NOT be filled */
  ASSERT(!pixel_visible(c, 50, 5));
  /* Total visible area should be reasonable */
  uint32_t visible = count_visible_pixels(c);
  ASSERT(visible > 2000);
  ASSERT(visible < 5000);

  facet_path_free(p);
  facet_canvas_free(c);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Anti-Aliasing Quality Tests
 * ═══════════════════════════════════════════════════════════════════════════
 */

TEST(test_aa_edge_has_partial_alpha) {
  /* A diagonal edge should produce partial alpha (AA) pixels */
  FacetCanvas *c = facet_canvas_new(100, 100);
  facet_canvas_clear(c, 0x00000000);

  FacetPath *p = facet_path_new();
  facet_path_move_to(p, 0.0f, 0.0f);
  facet_path_line_to(p, 100.0f, 0.0f);
  facet_path_line_to(p, 100.0f, 100.0f);
  facet_path_close(p);

  facet_canvas_fill_nonzero(c, p, 0xFFFFFFFF);

  /* The diagonal edge should have some pixels with partial alpha (0 < alpha <
   * 255) */
  int has_partial = 0;
  for (uint32_t y = 10; y < 90; y++) {
    for (uint32_t x = 0; x < 100; x++) {
      uint8_t a = c->pixels[y * c->stride + x * 4 + 3];
      if (a > 0 && a < 255) {
        has_partial = 1;
        break;
      }
    }
    if (has_partial)
      break;
  }
  ASSERT(has_partial);

  facet_path_free(p);
  facet_canvas_free(c);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════
 */

int main(void) {
  printf("Facet Raster — Test Suite\n");
  printf("═══════════════════════════════════════════════════════════════\n\n");

  printf("Path Construction:\n");
  RUN_TEST(test_path_create_free);
  RUN_TEST(test_path_move_and_line);
  RUN_TEST(test_path_quad);
  RUN_TEST(test_path_cubic);
  RUN_TEST(test_path_close);
  RUN_TEST(test_path_capacity_growth);

  printf("\nCanvas:\n");
  RUN_TEST(test_canvas_create);
  RUN_TEST(test_canvas_clear);
  RUN_TEST(test_canvas_clear_transparent);

  printf("\nBézier Flattening:\n");
  RUN_TEST(test_flatten_quad_degenerate);
  RUN_TEST(test_flatten_quad_arc);
  RUN_TEST(test_flatten_cubic);
  RUN_TEST(test_flatten_tolerance_coarse);

  printf("\nFill:\n");
  RUN_TEST(test_fill_triangle);
  RUN_TEST(test_fill_rectangle);
  RUN_TEST(test_fill_circle_approx);
  RUN_TEST(test_fill_winding_nonzero);
  RUN_TEST(test_fill_winding_evenodd);
  RUN_TEST(test_canvas_bounds_safety);
  RUN_TEST(test_fill_empty_path);
  RUN_TEST(test_fill_single_point);
  RUN_TEST(test_fill_multiple_subpaths);
  RUN_TEST(test_fill_with_curves);

  printf("\nAnti-Aliasing:\n");
  RUN_TEST(test_aa_edge_has_partial_alpha);

  printf("\n═══════════════════════════════════════════════════════════════\n");
  printf("Results: %d/%d passed", g_tests_passed, g_tests_run);
  if (g_tests_failed > 0) {
    printf(" (%d FAILED)", g_tests_failed);
  }
  printf("\n");

  return g_tests_failed > 0 ? 1 : 0;
}
