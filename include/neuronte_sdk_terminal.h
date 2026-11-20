/* SPDX-License-Identifier: MIT */
#ifndef NEURONTE_SDK_TERMINAL_H
#define NEURONTE_SDK_TERMINAL_H

#include "neuronte.h"

#include <stdint.h>

/* Terminal faces for an embedded NeuroTerm.
 *
 * The caller keeps the configured base handle (room art, room text). This
 * set does not close it. Supplemental files are owned here: newer
 * neuronte-split-vN bundles, neuronte-term-* bundles, and neuronte-emoji-vN.
 * A scalar is trained when a loaded file's range table contains it.
 * neuronte_untrained() cannot see a two-cell emoji's right half, because
 * that private code is above U+10FFFF and the ordinary calls replace it
 * with U+FFFD, so half membership is read from the bundle table.
 *
 * Compile this .c into the program that links Neuronte. It is not part of
 * libneuronte-sdk.
 */

typedef struct nsdk_terminal nsdk_terminal;

/* base is the already-open configured weights. base_path is that file,
 * used to read its range table and to find sibling bundles. */
int nsdk_terminal_open(neuronte *base, const char *base_path, nsdk_terminal **out);
void nsdk_terminal_close(nsdk_terminal *t);

/* 1 when some loaded table contains scalar, or both halves of a two-cell
 * emoji. 0 when only the base gate would draw it. */
int nsdk_terminal_trained(const nsdk_terminal *t, uint32_t scalar);

/* Draw one terminal glyph. span is 1 or 2. A two-cell emoji whose table
 * contains scalar and 0x110000+scalar is drawn as two cell_w x cell_h
 * halves starting at x, emoji before a monochrome hit. Otherwise the first
 * trained file draws one cell at (x, y). The base is first, so a scalar it
 * was trained on stays on that file. An untrained scalar is drawn with the
 * base. color is 0xAARRGGBB. style is NEURONTE_STYLE_*. */
int nsdk_terminal_draw(nsdk_terminal *t, uint32_t *pixels, int stride, int width, int height,
                       int x, int y, int cell_w, int cell_h, int span, uint32_t scalar,
                       uint32_t color, unsigned style);

#endif
