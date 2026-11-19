/* SPDX-License-Identifier: MIT */
#include "neuronte_sdk_mask.h"

#include <string.h>

static uint8_t coverage_byte(float coverage)
{
    return coverage <= 0.f ? 0 : coverage >= 1.f ? 255 :
           (uint8_t)(coverage * 255.f + 0.5f);
}

int nsdk_cell_mask(const neuronte *face, uint32_t scalar, unsigned style,
                   int trained, int span, int cell_w, int cell_h,
                   float *scratch, uint8_t *mask)
{
    if (!face || !scratch || !mask || (span != 1 && span != 2) ||
        cell_w < 1 || cell_w > 512 || cell_h < 1 || cell_h > 1024)
        return NEURONTE_ERR_ARG;

    int out_w = span * cell_w;
    int draw_w = trained ? cell_w : out_w;
    int rc = neuronte_coverage_styled(face, scalar, draw_w, cell_h, 0,
                                      trained ? style : 0, scratch);
    if (rc != NEURONTE_OK)
        return rc;

    int left = (out_w - draw_w) / 2;
    if (out_w != draw_w)
        memset(mask, 0, (size_t)out_w * (size_t)cell_h);
    for (int y = 0; y < cell_h; y++)
        for (int x = 0; x < draw_w; x++)
            mask[(size_t)y * (size_t)out_w + (size_t)(left + x)] =
                coverage_byte(scratch[(size_t)y * (size_t)draw_w + (size_t)x]);
    return NEURONTE_OK;
}
