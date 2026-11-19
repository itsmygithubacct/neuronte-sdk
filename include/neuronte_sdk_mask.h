/* SPDX-License-Identifier: MIT */
#ifndef NEURONTE_SDK_MASK_H
#define NEURONTE_SDK_MASK_H

#include "neuronte.h"

/* Render an 8-bit row-major mask using caller-owned buffers. Both buffers
 * hold span * cell_w * cell_h samples. span is 1 or 2. Centre a trained
 * glyph in a wide cell; draw an untrained mark across the whole span. */
int nsdk_cell_mask(const neuronte *face, uint32_t scalar, unsigned style,
                   int trained, int span, int cell_w, int cell_h,
                   float *scratch, uint8_t *mask);

#endif
