/*
 * Facet Raster — Resolution-Independent Path Rasterizer
 *
 * Core rendering engine for the Facet compositor.
 * Implements adaptive Bézier flattening, signed-area coverage accumulation,
 * and anti-aliased scanline fill.
 *
 * Design decisions:
 *   - Signed-area coverage (not supersampling) for quality/speed balance
 *   - Adaptive de Casteljau flattening with configurable tolerance
 *   - Non-zero winding rule by default (matches SVG/PostScript)
 *   - Pre-multiplied alpha throughout the pipeline
 */

#ifndef FACET_RASTER_H
#define FACET_RASTER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  float x, y;
} FacetPoint;

typedef struct {
  FacetPoint p0, p1, p2; /* Quadratic: start, control, end */
} FacetQuadBezier;

typedef struct {
  FacetPoint p0, p1, p2, p3; /* Cubic: start, ctrl1, ctrl2, end */
} FacetCubicBezier;

/* Winding rule for fill operations */
typedef enum {
  FACET_WINDING_NONZERO = 0,
  FACET_WINDING_EVENODD = 1,
} FacetWindingRule;

/* Path command types (internal) */
typedef enum {
  FACET_CMD_MOVE_TO,
  FACET_CMD_LINE_TO,
  FACET_CMD_QUAD_TO,
  FACET_CMD_CUBIC_TO,
  FACET_CMD_CLOSE,
} FacetPathCmdType;

typedef struct {
  FacetPathCmdType type;
  FacetPoint points[3]; /* Max 3 control points (cubic) */
} FacetPathCmd;

/* Path: an ordered list of sub-paths (move_to starts a new sub-path) */
typedef struct {
  FacetPathCmd *cmds;
  size_t count;
  size_t capacity;
} FacetPath;

/* Canvas: an RGBA pixel buffer */
typedef struct {
  uint8_t *pixels; /* RGBA, pre-multiplied alpha */
  uint32_t width;
  uint32_t height;
  uint32_t stride; /* Bytes per row (>= width * 4) */
} FacetCanvas;

/* ═══════════════════════════════════════════════════════════════════════════
 * Path Construction
 * ═══════════════════════════════════════════════════════════════════════════
 */

FacetPath *facet_path_new(void);
void facet_path_move_to(FacetPath *path, float x, float y);
void facet_path_line_to(FacetPath *path, float x, float y);
void facet_path_quad_to(FacetPath *path, float cx, float cy, float x, float y);
void facet_path_cubic_to(FacetPath *path, float c1x, float c1y, float c2x,
                         float c2y, float x, float y);
void facet_path_close(FacetPath *path);
void facet_path_free(FacetPath *path);

/* ═══════════════════════════════════════════════════════════════════════════
 * Canvas
 * ═══════════════════════════════════════════════════════════════════════════
 */

FacetCanvas *facet_canvas_new(uint32_t width, uint32_t height);
void facet_canvas_clear(FacetCanvas *canvas, uint32_t rgba);
void facet_canvas_free(FacetCanvas *canvas);

/* ═══════════════════════════════════════════════════════════════════════════
 * Rasterization
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* Fill a path onto the canvas with the given RGBA color.
 * Uses signed-area coverage with the specified winding rule. */
void facet_canvas_fill(FacetCanvas *canvas, const FacetPath *path,
                       uint32_t rgba, FacetWindingRule rule);

/* Fill with default non-zero winding rule (convenience) */
void facet_canvas_fill_nonzero(FacetCanvas *canvas, const FacetPath *path,
                               uint32_t rgba);

/* ═══════════════════════════════════════════════════════════════════════════
 * Bézier Utilities (exposed for testing)
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* Flatten a quadratic Bézier into line segments.
 * Calls callback(x, y, user_data) for each point.
 * tolerance: maximum deviation in pixels (0.25 is a good default). */
typedef void (*FacetFlattenCallback)(float x, float y, void *user_data);

void facet_flatten_quad(FacetPoint p0, FacetPoint p1, FacetPoint p2,
                        float tolerance, FacetFlattenCallback callback,
                        void *user_data);

void facet_flatten_cubic(FacetPoint p0, FacetPoint p1, FacetPoint p2,
                         FacetPoint p3, float tolerance,
                         FacetFlattenCallback callback, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* FACET_RASTER_H */
