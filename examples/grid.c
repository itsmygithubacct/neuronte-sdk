/* Draw Hello, a light box, and a full block into an 8 by 16 cell grid
 * and write a PPM. Cells are the Kilix 8 by 16. No terminal. */
#include "neuronte_sdk.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

enum {
    COLS = 8,
    ROWS = 16,
    CELL_W = 8,
    CELL_H = 16
};

static int fail(const char *message)
{
    (void)fprintf(stderr, "grid: %s\n", message);
    return EXIT_FAILURE;
}

static int write_ppm(const char *path, const uint8_t *rgb, int width, int height)
{
    FILE *file = fopen(path, "wb");
    size_t count;

    if (!file)
        return -1;
    if (fprintf(file, "P6\n%d %d\n255\n", width, height) < 0) {
        (void)fclose(file);
        return -1;
    }
    count = (size_t)width * (size_t)height * 3u;
    if (fwrite(rgb, 1, count, file) != count) {
        (void)fclose(file);
        return -1;
    }
    if (fclose(file) != 0)
        return -1;
    return 0;
}

static int has_ink(const uint32_t *pixels, int count, uint32_t background)
{
    int i;

    for (i = 0; i < count; i++) {
        if (pixels[i] != background)
            return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *path = "grid.ppm";
    const uint32_t background = 0xFF101010u;
    const uint32_t ink = 0xFFE7E5E4u;
    const uint32_t rule = 0xFF8AA0A8u;
    const uint32_t block = 0xFF66CCFFu;
    const int width = COLS * CELL_W;
    const int height = ROWS * CELL_H;
    const int count = width * height;
    nsdk *face = NULL;
    uint32_t *pixels;
    uint8_t *rgb;
    int i, rc;

    if (argc > 2)
        return fail("usage: grid [output.ppm]");
    if (argc == 2)
        path = argv[1];

    pixels = malloc((size_t)count * sizeof *pixels);
    rgb = malloc((size_t)count * 3u);
    if (!pixels || !rgb) {
        free(pixels);
        free(rgb);
        return fail("out of memory");
    }
    for (i = 0; i < count; i++)
        pixels[i] = background;

    rc = nsdk_open(NULL, &face);
    if (rc != NEURONTE_OK) {
        free(pixels);
        free(rgb);
        return fail("could not open neuronte-split-v4.nrt (set NEURONTE_HOME or NEURONTE_WEIGHTS)");
    }

    rc = nsdk_grid_text(face, pixels, width, width, height, 0, 0, CELL_W, CELL_H,
                        0, 0, "Hello", 5, ink);
    if (rc == 5) {
        rc = nsdk_grid_put(face, pixels, width, width, height, 0, 0, CELL_W, CELL_H,
                           0, 2, 0x250Cu, rule);
    }
    if (rc == NEURONTE_OK)
        rc = nsdk_grid_put(face, pixels, width, width, height, 0, 0, CELL_W, CELL_H,
                           1, 2, 0x2500u, rule);
    if (rc == NEURONTE_OK)
        rc = nsdk_grid_put(face, pixels, width, width, height, 0, 0, CELL_W, CELL_H,
                           2, 2, 0x2510u, rule);
    if (rc == NEURONTE_OK)
        rc = nsdk_grid_put(face, pixels, width, width, height, 0, 0, CELL_W, CELL_H,
                           0, 3, 0x2502u, rule);
    if (rc == NEURONTE_OK)
        rc = nsdk_grid_put(face, pixels, width, width, height, 0, 0, CELL_W, CELL_H,
                           2, 3, 0x2502u, rule);
    if (rc == NEURONTE_OK)
        rc = nsdk_grid_put(face, pixels, width, width, height, 0, 0, CELL_W, CELL_H,
                           1, 3, 0x2588u, block);
    if (rc == NEURONTE_OK)
        rc = nsdk_grid_put(face, pixels, width, width, height, 0, 0, CELL_W, CELL_H,
                           0, 4, 0x2514u, rule);
    if (rc == NEURONTE_OK)
        rc = nsdk_grid_put(face, pixels, width, width, height, 0, 0, CELL_W, CELL_H,
                           1, 4, 0x2500u, rule);
    if (rc == NEURONTE_OK)
        rc = nsdk_grid_put(face, pixels, width, width, height, 0, 0, CELL_W, CELL_H,
                           2, 4, 0x2518u, rule);
    if (rc != NEURONTE_OK) {
        nsdk_close(face);
        free(pixels);
        free(rgb);
        return fail("draw failed");
    }
    if (!has_ink(pixels, count, background)) {
        nsdk_close(face);
        free(pixels);
        free(rgb);
        return fail("drawing left the grid blank");
    }
    if (nsdk_rgb(pixels, width, height, rgb, (size_t)count * 3u) != NEURONTE_OK ||
        write_ppm(path, rgb, width, height) != 0) {
        nsdk_close(face);
        free(pixels);
        free(rgb);
        return fail("could not write the PPM");
    }

    nsdk_close(face);
    free(pixels);
    free(rgb);
    return EXIT_SUCCESS;
}
