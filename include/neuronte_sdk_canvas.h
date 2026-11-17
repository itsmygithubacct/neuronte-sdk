#ifndef NEURONTE_SDK_CANVAS_H
#define NEURONTE_SDK_CANVAS_H

/*
 * A pixel canvas for programs that draw with Neuronte.
 *
 * It holds what such a program still does in plain code: make and free
 * the pixel buffer, set a clip, clear the frame, fill a rectangle with one
 * colour, and hand the pixels on as RGBA bytes or a PPM file. It draws no
 * other shape; shapes come from a net.
 *
 * The pixels are 0xAARRGGBB, row after row, px[y * w + x]: the layout
 * Neuronte's draw calls and nsdk_draw_scalar() take, with w as the stride.
 * The struct has the fields of soft-raster's sr_canvas in the same order,
 * and each function gives the same bytes as the soft-raster function
 * named beside it.
 *
 * This header and its source need no weight file and no libneuronte. It
 * may be included beside neuronte_sdk.h or neuronte.h.
 *
 * Copyright (c) 2026 neuronte-sdk contributors
 * MIT licence. See LICENSE. The source is adapted from soft-raster and
 * carries that project's notice.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nsdk_canvas {
    uint32_t *px; /* row-major 0xAARRGGBB, px[y * w + x] */
    int w;
    int h;
    int clip_x0; /* fills touch x in [clip_x0, clip_x1) and y in [clip_y0, clip_y1) */
    int clip_y0;
    int clip_x1;
    int clip_y1;
    bool owns_px; /* set by nsdk_canvas_init, cleared by nsdk_canvas_wrap */
} nsdk_canvas;

/* A zeroed w x h buffer with the whole canvas as the clip. False, and an
 * empty canvas, when a side is not positive, the pixel count passes
 * INT_MAX, or there is no memory. (sr_canvas_init) */
bool nsdk_canvas_init(nsdk_canvas *c, int w, int h);

/* Use the caller's w x h pixels. The canvas does not own them. A null
 * buffer or a bad size leaves the canvas empty. (sr_canvas_wrap) */
void nsdk_canvas_wrap(nsdk_canvas *c, uint32_t *mem, int w, int h);

/* Free the buffer if nsdk_canvas_init made it, and empty the canvas. An
 * empty canvas is ignored by every call here. (sr_canvas_free) */
void nsdk_canvas_free(nsdk_canvas *c);

/* The clip is the rectangle cut to the canvas. A width or height that is
 * not positive gives an empty clip. (sr_canvas_set_clip, sr_canvas_reset_clip) */
void nsdk_canvas_set_clip(nsdk_canvas *c, int x, int y, int w, int h);
void nsdk_canvas_reset_clip(nsdk_canvas *c);

/* Every pixel becomes the opaque colour 0xRRGGBB. The clip does not bound
 * a clear. (sr_clear) */
void nsdk_canvas_clear(nsdk_canvas *c, uint32_t rgb);

/* Blend the colour 0xRRGGBB over the rectangle, inside the clip. A pixel
 * the rectangle partly covers takes that part of alpha, so an edge may lie
 * between pixels. Alpha above 1 is 1. A value that is not finite, a side
 * that is not positive and an alpha that is not above 0 draw nothing.
 * (sr_fill_rect) */
void nsdk_canvas_fill_rect(nsdk_canvas *c, float x, float y, float w, float h,
                           uint32_t rgb, float alpha);

/* The pixels as bytes R, G, B, A. byte_count must be at least w * h * 4.
 * (sr_pack_rgba) */
bool nsdk_canvas_pack_rgba(const nsdk_canvas *c, uint8_t *rgba, size_t byte_count);

/* A binary PPM, three bytes a pixel; alpha is not written. On failure
 * errno says why and a part file may be left. (sr_write_ppm) */
bool nsdk_canvas_write_ppm(const nsdk_canvas *c, const char *path);

#ifdef __cplusplus
}
#endif

#endif
