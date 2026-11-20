/* SPDX-License-Identifier: MIT */
#ifndef NEURONTE_SDK_TERMINAL_H
#define NEURONTE_SDK_TERMINAL_H

#include "neuronte.h"

#include <stddef.h>
#include <stdint.h>

/* Terminal faces for an embedded NeuroTerm.
 *
 * The first face is the caller's base (room art, room text). This set
 * closes a face only when that face was added with owned set. Supplemental
 * files are newer neuronte-split-vN bundles, neuronte-term-* bundles, and
 * neuronte-emoji-vN. A scalar is trained when a loaded file's range table
 * contains it. neuronte_untrained() cannot see a two-cell emoji's right
 * half, because that private code is above U+10FFFF and the ordinary calls
 * replace it with U+FFFD, so half membership is read from the bundle table.
 *
 * The opener is part of libneuronte-sdk. Link libneuronte as well: the
 * archive calls neuronte_* and does not dlopen the library. The grid API
 * (nsdk_open) still loads one weight file.
 */

typedef struct nsdk_terminal nsdk_terminal;

/* An empty set. The first nsdk_terminal_add is the base. */
int nsdk_terminal_create(nsdk_terminal **out);

/* Add an already-open face. name selects the role for every face after the
 * base (neuronte-split-vN, neuronte-term-*, neuronte-emoji-vN). bytes/len
 * are the weight blob, used only to read the range table; NULL keeps the
 * base with no table. owned non-zero means nsdk_terminal_close closes the
 * face. Returns NEURONTE_OK when the face is in the set, 1 when a
 * supplement is ignored (unknown name, no usable table, or the set is
 * full), or a negative Neuronte error. An ignored owned face is closed. */
int nsdk_terminal_add(nsdk_terminal *t, neuronte *face, const char *name,
                      const void *bytes, size_t len, int owned);

/* Same as nsdk_terminal_add. The range table is read from path, and the
 * file name selects the role. The face must already be open. */
int nsdk_terminal_add_path(nsdk_terminal *t, neuronte *face, const char *path, int owned);

/* Reorder to base, then splits (highest version first, as added), terms,
 * then emoji. Draw and membership walk that order. */
int nsdk_terminal_finish(nsdk_terminal *t);

/* base is the already-open configured weights. base_path is that file,
 * used to read its range table and to find sibling bundles. The base is
 * not owned. This calls create, add, the directory search, and finish. */
int nsdk_terminal_open(neuronte *base, const char *base_path, nsdk_terminal **out);
void nsdk_terminal_close(nsdk_terminal *t);

/* 1 when some loaded table contains scalar, or both halves of a two-cell
 * emoji. 0 when only the base gate would draw it. */
int nsdk_terminal_trained(const nsdk_terminal *t, uint32_t scalar);

/* The file that draws scalar in one cell, or NULL when the base would draw
 * the untrained mark. A two-cell emoji reports the file that holds the
 * scalar. */
const char *nsdk_terminal_name(const nsdk_terminal *t, uint32_t scalar);

/* Draw one terminal glyph. span is 1 or 2. A two-cell emoji whose table
 * contains scalar and 0x110000+scalar is drawn as two cell_w x cell_h
 * halves starting at x, emoji before a monochrome hit. Otherwise the first
 * trained file draws one cell at (x, y). The base is first, so a scalar it
 * was trained on stays on that file. An untrained scalar is drawn with the
 * base. color is 0xAARRGGBB. style is NEURONTE_STYLE_*. */
int nsdk_terminal_draw(nsdk_terminal *t, uint32_t *pixels, int stride, int width, int height,
                       int x, int y, int cell_w, int cell_h, int span, uint32_t scalar,
                       uint32_t color, unsigned style);

/* Fill a row-major 8-bit mask of span * cell_w by cell_h. scratch holds at
 * least that many floats. A half pair (span 2 and both codes in one file)
 * is the left half then the right half, not centred. Any other trained
 * scalar is one cell centred in the span. An untrained scalar is the base
 * mark across the whole span. */
int nsdk_terminal_mask(const nsdk_terminal *t, uint32_t scalar, unsigned style, int span,
                       int cell_w, int cell_h, float *scratch, uint8_t *mask);

#endif
