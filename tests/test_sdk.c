#include "neuronte_sdk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                      \
    do {                                                                      \
        if (!(condition)) {                                                   \
            (void)fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__,     \
                          __LINE__, #condition);                             \
            return 0;                                                         \
        }                                                                     \
    } while (0)

static void fill(uint32_t *pixels, int count, uint32_t color)
{
    int i;

    for (i = 0; i < count; i++)
        pixels[i] = color;
}

static int same(const uint32_t *a, const uint32_t *b, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        if (a[i] != b[i])
            return 0;
    }
    return 1;
}

static int cell_has_ink(const uint32_t *pixels, int stride, int x, int y,
                        int cell_w, int cell_h, uint32_t background)
{
    int row, col;

    for (row = 0; row < cell_h; row++) {
        for (col = 0; col < cell_w; col++) {
            if (pixels[(y + row) * stride + (x + col)] != background)
                return 1;
        }
    }
    return 0;
}

static int cell_is_clear(const uint32_t *pixels, int stride, int x, int y,
                         int cell_w, int cell_h, uint32_t background)
{
    return !cell_has_ink(pixels, stride, x, y, cell_w, cell_h, background);
}

/* A written pixel over opaque black is a rounded blend of one colour. */
static int one_hue(uint32_t pixel, uint32_t color)
{
    int channels[3];
    int sources[3];
    int pair;

    if ((pixel >> 24) != 0xFFu)
        return 0;
    channels[0] = (int)((pixel >> 16) & 0xFFu);
    channels[1] = (int)((pixel >> 8) & 0xFFu);
    channels[2] = (int)(pixel & 0xFFu);
    sources[0] = (int)((color >> 16) & 0xFFu);
    sources[1] = (int)((color >> 8) & 0xFFu);
    sources[2] = (int)(color & 0xFFu);
    for (pair = 0; pair < 3; pair++) {
        if (channels[pair] > sources[pair])
            return 0;
    }
    for (pair = 0; pair < 3; pair++) {
        int i = pair;
        int j = (pair + 1) % 3;
        long cross = (long)channels[i] * sources[j] - (long)channels[j] * sources[i];
        long limit = ((long)sources[i] + sources[j]) / 2 + 1;

        if (cross < 0)
            cross = -cross;
        if (cross > limit)
            return 0;
    }
    return 1;
}

static int test_rgb(void)
{
    uint32_t pixels[2] = {0xFF112233u, 0x80AABBCCu};
    uint8_t rgb[6];

    CHECK(nsdk_rgb(pixels, 2, 1, rgb, sizeof rgb) == NEURONTE_OK);
    CHECK(rgb[0] == 0x11 && rgb[1] == 0x22 && rgb[2] == 0x33);
    CHECK(rgb[3] == 0xAA && rgb[4] == 0xBB && rgb[5] == 0xCC);
    CHECK(nsdk_rgb(pixels, 2, 1, rgb, 5) == NEURONTE_ERR_ARG);
    CHECK(nsdk_rgb(NULL, 1, 1, rgb, 3) == NEURONTE_ERR_ARG);
    CHECK(nsdk_rgb(pixels, -1, 1, rgb, sizeof rgb) == NEURONTE_ERR_ARG);
    CHECK(nsdk_rgb(pixels, 1, 1, NULL, 3) == NEURONTE_ERR_ARG);
    return 1;
}

static int test_null_args(void)
{
    uint32_t pixel = 0;
    char name[8];

    CHECK(nsdk_open(NULL, NULL) == NEURONTE_ERR_ARG);
    CHECK(nsdk_open("", NULL) == NEURONTE_ERR_ARG);
    nsdk_close(NULL);
    CHECK(nsdk_nets(NULL) == 0);
    CHECK(nsdk_route(NULL, 'A') == NEURONTE_ERR_ARG);
    CHECK(nsdk_net_name(NULL, 0, name, sizeof name) == NEURONTE_ERR_ARG);
    CHECK(nsdk_set_binary(NULL, 1) == NEURONTE_ERR_ARG);
    CHECK(nsdk_draw_scalar(NULL, &pixel, 1, 1, 1, 0, 0, 1, 1, 'A', 0) == NEURONTE_ERR_ARG);
    CHECK(nsdk_grid_put(NULL, &pixel, 1, 1, 1, 0, 0, 1, 1, 0, 0, 'A', 0) == NEURONTE_ERR_ARG);
    CHECK(nsdk_grid_text(NULL, &pixel, 1, 1, 1, 0, 0, 1, 1, 0, 0, "A", 1, 0) == NEURONTE_ERR_ARG);
    CHECK(nsdk_grid_fill(NULL, &pixel, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 'A', 0) == NEURONTE_ERR_ARG);
    return 1;
}

static int expect_row(const nsdk *face, const char *name, int index,
                      const uint32_t *scalars, int count)
{
    char got[32];
    int i;

    got[0] = '\0';
    for (i = 0; i < count; i++) {
        int route = nsdk_route(face, scalars[i]);

        if (route != index) {
            (void)fprintf(stderr, "route row %s: U+%04X -> %d, expected %d\n",
                          name, (unsigned)scalars[i], route, index);
            return 0;
        }
    }
    if (nsdk_net_name(face, index, got, sizeof got) != NEURONTE_OK ||
        strcmp(got, name) != 0) {
        (void)fprintf(stderr, "net name %d is '%s', expected '%s'\n", index, got, name);
        return 0;
    }
    return 1;
}

static int test_routes(const nsdk *face)
{
    const uint32_t ascii[] = {' ', 'A', '#'};
    const uint32_t draw[] = {0x2500u, 0x2502u, 0x250Cu, 0x2591u};
    const uint32_t symbol[] = {0x2192u};
    const uint32_t accent[] = {0x00E9u};
    const uint32_t pixel[] = {0x2588u, 0x2580u};
    const uint32_t braille[] = {0x2800u};
    char untouched[8];
    nsdk *missing = NULL;

    CHECK(nsdk_nets(face) == 6);
    CHECK(expect_row(face, "ascii", 0, ascii, 3));
    CHECK(expect_row(face, "draw", 1, draw, 4));
    CHECK(expect_row(face, "symbol", 2, symbol, 1));
    CHECK(expect_row(face, "accent", 3, accent, 1));
    CHECK(expect_row(face, "pixel", 4, pixel, 2));
    CHECK(expect_row(face, "braille", 5, braille, 1));

    memset(untouched, 'X', sizeof untouched);
    CHECK(nsdk_net_name(face, 6, untouched, sizeof untouched) == NEURONTE_ERR_ARG);
    CHECK(untouched[0] == 'X');
    CHECK(nsdk_net_name(face, -1, untouched, sizeof untouched) == NEURONTE_ERR_ARG);
    CHECK(nsdk_net_name(face, 0, untouched, 5) == NEURONTE_ERR_ARG);

    CHECK(nsdk_open("no-such-neuronte-weights.nrt", &missing) != NEURONTE_OK);
    CHECK(missing == NULL);
    return 1;
}

static int test_draw_a(nsdk *face)
{
    enum { W = 8, H = 16 };
    uint32_t pixels[W * H];
    const uint32_t background = 0xFF000000u;
    const uint32_t color = 0xFF66CCFFu;
    int i, changed = 0;

    fill(pixels, W * H, background);
    CHECK(nsdk_draw_scalar(face, pixels, W, W, H, 0, 0, W, H, 'A', color) == NEURONTE_OK);
    for (i = 0; i < W * H; i++) {
        if (pixels[i] == background)
            continue;
        changed++;
        if (!one_hue(pixels[i], color)) {
            (void)fprintf(stderr, "pixel %08X is not a blend of %08X\n",
                          pixels[i], color);
            return 0;
        }
    }
    CHECK(changed > 0);
    return 1;
}

static int test_binary(nsdk *face)
{
    enum { W = 8, H = 16 };
    uint32_t pixels[W * H];
    const uint32_t background = 0xFF000000u;
    const uint32_t color = 0xFF66CCFFu;
    int i, ink = 0;

    CHECK(nsdk_set_binary(face, 2) == NEURONTE_ERR_ARG);
    CHECK(nsdk_set_binary(face, 1) == NEURONTE_OK);
    fill(pixels, W * H, background);
    CHECK(nsdk_draw_scalar(face, pixels, W, W, H, 0, 0, W, H, 'A', color) == NEURONTE_OK);
    for (i = 0; i < W * H; i++) {
        if (pixels[i] == background)
            continue;
        ink++;
        CHECK(pixels[i] == color);
    }
    CHECK(ink > 0);
    CHECK(nsdk_set_binary(face, 0) == NEURONTE_OK);
    return 1;
}

static int test_grid(nsdk *face)
{
    enum { CW = 8, CH = 16, COLS = 4, ROWS = 3 };
    enum { W = COLS * CW, H = ROWS * CH };
    uint32_t text[W * H], scalar[W * H], bad[W * H], wide[32 * CH];
    const uint32_t background = 0xFF101010u;
    const uint32_t color = 0xFF66CCFFu;
    const char e_acute[] = "\xC3\xA9";
    const char broken[] = "\xFF";
    int rc;

    fill(text, W * H, background);
    rc = nsdk_grid_text(face, text, W, W, H, 0, 0, CW, CH, 1, 1, "Hi", 2, color);
    CHECK(rc == 2);
    CHECK(cell_is_clear(text, W, 0, CH, CW, CH, background));
    CHECK(cell_has_ink(text, W, CW, CH, CW, CH, background));
    CHECK(cell_has_ink(text, W, 2 * CW, CH, CW, CH, background));
    CHECK(cell_is_clear(text, W, 3 * CW, CH, CW, CH, background));

    fill(wide, 32 * CH, background);
    fill(scalar, 32 * CH, background);
    CHECK(nsdk_grid_text(face, wide, 32, 32, CH, 0, 0, CW, CH, 0, 0,
                         e_acute, sizeof e_acute - 1u, color) == 1);
    CHECK(nsdk_draw_scalar(face, scalar, 32, 32, CH, 0, 0, CW, CH, 0x00E9u, color) ==
          NEURONTE_OK);
    CHECK(same(wide, scalar, 32 * CH));
    CHECK(cell_is_clear(wide, 32, CW, 0, CW, CH, background));

    fill(bad, 32 * CH, background);
    fill(scalar, 32 * CH, background);
    CHECK(nsdk_grid_text(face, bad, 32, 32, CH, 0, 0, CW, CH, 0, 0,
                         broken, 1, color) == 1);
    CHECK(nsdk_draw_scalar(face, scalar, 32, 32, CH, 0, 0, CW, CH, 0xFFFDu, color) ==
          NEURONTE_OK);
    CHECK(same(bad, scalar, 32 * CH));

    fill(text, W * H, background);
    CHECK(nsdk_grid_fill(face, text, W, W, H, 0, 0, CW, CH, 1, 0, 2, 2,
                         0x2588u, color) == NEURONTE_OK);
    CHECK(cell_is_clear(text, W, 0, 0, CW, CH, background));
    CHECK(cell_has_ink(text, W, CW, 0, CW, CH, background));
    CHECK(cell_has_ink(text, W, 2 * CW, 0, CW, CH, background));
    CHECK(cell_has_ink(text, W, CW, CH, CW, CH, background));
    CHECK(cell_has_ink(text, W, 2 * CW, CH, CW, CH, background));
    CHECK(cell_is_clear(text, W, 0, 2 * CH, CW, CH, background));

    fill(text, CW * CH, background);
    CHECK(nsdk_grid_put(face, text, CW, CW, CH, 0, 0, CW, CH, 2, 0, 'A', color) ==
          NEURONTE_OK);
    CHECK(cell_is_clear(text, CW, 0, 0, CW, CH, background));
    CHECK(nsdk_grid_put(face, text, CW, CW, CH, 0, 0, CW, CH, -1, 0, 'A', color) ==
          NEURONTE_OK);
    CHECK(cell_is_clear(text, CW, 0, 0, CW, CH, background));
    CHECK(nsdk_grid_text(face, text, CW, CW, CH, 0, 0, CW, CH, 0, 0, NULL, 0, color) == 0);
    CHECK(nsdk_grid_text(face, text, CW, CW, CH, 0, 0, CW, CH, 0, 0, NULL, 1, color) ==
          NEURONTE_ERR_ARG);
    return 1;
}

int main(void)
{
    nsdk *face = NULL;
    int rc;

    if (!test_rgb() || !test_null_args())
        return EXIT_FAILURE;

    rc = nsdk_open(NULL, &face);
    if (rc == NEURONTE_ERR_IO) {
        (void)fprintf(stderr,
                      "SKIP: libneuronte or neuronte-split-v4.nrt is absent. "
                      "Set NEURONTE_LIB and NEURONTE_WEIGHTS, or NEURONTE_HOME "
                      "or NEURONTE_DIR.\n");
        return EXIT_SUCCESS;
    }
    if (rc != NEURONTE_OK || !face) {
        (void)fprintf(stderr, "nsdk_open failed: %d\n", rc);
        return EXIT_FAILURE;
    }
    if (!test_routes(face) || !test_draw_a(face) || !test_grid(face) || !test_binary(face)) {
        nsdk_close(face);
        return EXIT_FAILURE;
    }
    nsdk_close(face);
    (void)printf("neuronte-sdk tests passed\n");
    return EXIT_SUCCESS;
}
