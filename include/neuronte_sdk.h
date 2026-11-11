#ifndef NEURONTE_SDK_H
#define NEURONTE_SDK_H

/*
 * A cell surface on top of libneuronte.
 *
 * The caller owns a grid of Unicode scalars and 0xAARRGGBB colours. Each
 * cell is drawn with neuronte_draw_scalar into a caller-owned 0xAARRGGBB
 * buffer, the same pixel layout as Neuronte and soft-raster. Packed RGB
 * bytes from nsdk_rgb() are what a frame presenter consumes.
 *
 * This library does not open a window, take a terminal, read the keyboard,
 * or talk to a model. It does not ship weights. nsdk_open(NULL) searches
 * for neuronte-split-v5.nrt (falling back to v4) and libneuronte.so.
 *
 * Status codes are Neuronte's. Include this header or neuronte.h, not both:
 * both repeat the same enumeration.
 *
 * Copyright (c) 2026 neuronte-sdk contributors
 * MIT licence. See LICENSE.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    NEURONTE_OK = 0,
    NEURONTE_ERR_ARG = -1,    /* a null pointer or a size out of range */
    NEURONTE_ERR_FORMAT = -2, /* not a Neuronte weight file, or damaged */
    NEURONTE_ERR_SHAPE = -3,  /* a weight file for a different net shape */
    NEURONTE_ERR_NOMEM = -4,
    NEURONTE_ERR_IO = -5
};

typedef struct nsdk nsdk;

/* weights_or_null NULL uses the weight search. *out is NULL on failure. */
int nsdk_open(const char *weights_or_null, nsdk **out);
void nsdk_close(nsdk *s);

/* Draw nets in the file, or 0 when s is NULL. The router's gate is not one. */
int nsdk_nets(const nsdk *s);

/* Net index, or a negative Neuronte error. */
int nsdk_route(const nsdk *s, uint32_t scalar);

/* When the file has six draw nets, indexes 0..5 are ascii, draw, symbol,
 * accent, pixel and braille. Any other index is netN. The gate is not a
 * draw net and is not named. Returns NEURONTE_OK, or a negative error.
 * buf is unchanged on error. */
int nsdk_net_name(const nsdk *s, int index, char *buf, size_t len);

/* 1 cuts distance-net coverage at one half. 0, the default, blends.
 * A colour glyph is still composited by coverage. */
int nsdk_set_binary(nsdk *s, int on);

/* Draw one scalar. pixels is 0xAARRGGBB, stride in pixels. Colour is
 * 0xAARRGGBB. Coverage blends the caller's RGB. Binary cut is off unless
 * nsdk_set_binary(s, 1). */
int nsdk_draw_scalar(nsdk *s, uint32_t *pixels, int stride, int width, int height,
                     int x, int y, int cell_w, int cell_h,
                     uint32_t scalar, uint32_t color);

/* Grid helpers. Origin is the top-left cell. Cells past the edge are clipped.
 * nsdk_grid_text returns the number of cells, or a negative Neuronte error.
 * It decodes UTF-8 with neuronte_utf8_decode. One cell per scalar. No wrap,
 * no wide-character doubling. */
int nsdk_grid_put(nsdk *s, uint32_t *pixels, int stride, int width, int height,
                  int origin_x, int origin_y, int cell_w, int cell_h,
                  int col, int row, uint32_t scalar, uint32_t color);
int nsdk_grid_text(nsdk *s, uint32_t *pixels, int stride, int width, int height,
                   int origin_x, int origin_y, int cell_w, int cell_h,
                   int col, int row, const char *utf8, size_t len, uint32_t color);
int nsdk_grid_fill(nsdk *s, uint32_t *pixels, int stride, int width, int height,
                   int origin_x, int origin_y, int cell_w, int cell_h,
                   int col, int row, int cols, int rows,
                   uint32_t scalar, uint32_t color);

/* Pack 0xAARRGGBB rows into RGB888 bytes. out_len is width*height*3.
 * pixels are tightly packed (one row follows the next). */
int nsdk_rgb(const uint32_t *pixels, int width, int height, uint8_t *out, size_t out_len);

#ifdef __cplusplus
}
#endif

#endif
