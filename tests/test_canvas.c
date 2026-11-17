/* The canvas, checked two ways. Results that can be worked out by hand are
 * written as numbers. The rest (twenty thousand clips, sixty thousand
 * fills, a packed frame, a PPM) are folded into sums and compared with the
 * sums soft-raster 0.5.0 gives for the same calls, which are pinned below.
 *
 * To print the pinned table again from a soft-raster checkout:
 *     make canvas-reference SOFT_RASTER=/path/to/soft-raster
 * That builds this file against soft-raster in place of the SDK. */
#include <errno.h>
#include <limits.h>
#ifdef __GLIBC__
#include <malloc.h>
#endif
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef NSDK_CANVAS_REFERENCE
#include "soft_raster.h"
typedef sr_canvas nsdk_canvas;
#define nsdk_canvas_init sr_canvas_init
#define nsdk_canvas_wrap sr_canvas_wrap
#define nsdk_canvas_free sr_canvas_free
#define nsdk_canvas_set_clip sr_canvas_set_clip
#define nsdk_canvas_reset_clip sr_canvas_reset_clip
#define nsdk_canvas_clear sr_clear
#define nsdk_canvas_fill_rect sr_fill_rect
#define nsdk_canvas_pack_rgba sr_pack_rgba
#define nsdk_canvas_write_ppm sr_write_ppm
#else
#include "neuronte_sdk_canvas.h"
#endif

#ifndef NSDK_CANVAS_REFERENCE
/* What soft-raster 0.5.0 gives. Printed by `make canvas-reference`. */
static const struct { const char *name; uint64_t sum; } pinned[] = {
    {"buffer", 0xfd60876720a6d885ull},
    {"clip", 0xa346b88e58030711ull},
    {"fill 64x48", 0x2a940d33922bbf1eull},
    {"fill 33x17", 0x6ac75c0a7d5fea03ull},
    {"fill 64x48 again", 0x7fa331f8bf014050ull},
    {"fill 7x5", 0x49c0f8af4fe1360eull},
    {"fill 3x80", 0x5c28c757ec374d12ull},
    {"fill 1x1", 0xb8efeae37185d57eull},
    {"row past 2^24", 0xf8bd376ea55828e5ull},
    {"column past 2^24", 0xf8bd376ea55828e5ull},
    {"pack", 0xac0fb870e607bdefull},
    {"ppm", 0x7baade2909cc8e3cull},
};
#endif

static int failures;

static void expect(int ok, const char *what)
{
    if (!ok) {
        if (failures < 20) {
            fprintf(stderr, "FAIL: %s\n", what);
        }
        failures++;
    }
}

/* FNV-1a over 32-bit words. */
static uint64_t running;

static void fold(uint32_t word)
{
    int i;

    for (i = 0; i < 4; i++) {
        running ^= (word >> (8 * i)) & 255u;
        running *= 0x100000001b3ull;
    }
}

static void fold_start(void)
{
    running = 0xcbf29ce484222325ull;
}

static void fold_struct(const nsdk_canvas *c)
{
    fold((uint32_t)c->w);
    fold((uint32_t)c->h);
    fold((uint32_t)c->clip_x0);
    fold((uint32_t)c->clip_y0);
    fold((uint32_t)c->clip_x1);
    fold((uint32_t)c->clip_y1);
    fold((uint32_t)c->owns_px);
    fold(c->px != NULL);
}

static void fold_pixels(const nsdk_canvas *c)
{
    size_t n = (size_t)c->w * (size_t)c->h;
    size_t i;

    for (i = 0; i < n; i++) {
        fold(c->px[i]);
    }
}

/* Compare the running sum with the pinned one, or print it for the table. */
static void pin(const char *name)
{
#ifdef NSDK_CANVAS_REFERENCE
    printf("    {\"%s\", 0x%016llxull},\n", name, (unsigned long long)running);
#else
    size_t i;

    for (i = 0; i < sizeof pinned / sizeof pinned[0]; i++) {
        if (strcmp(pinned[i].name, name) == 0) {
            if (pinned[i].sum != running) {
                fprintf(stderr, "%s: sum %016llx, pinned %016llx\n", name,
                        (unsigned long long)running, (unsigned long long)pinned[i].sum);
            }
            expect(pinned[i].sum == running, name);
            return;
        }
    }
    expect(0, "a sum has no pinned value");
#endif
}

static uint32_t state = 0x2545f491u;

static uint32_t next(void)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

static uint32_t sum(const nsdk_canvas *c)
{
    size_t n = (size_t)c->w * (size_t)c->h;
    uint32_t total = 0u;
    size_t i;

    for (i = 0; i < n; i++) {
        total = total * 31u + c->px[i];
    }
    return total;
}

/* Arbitrary pixels, alpha included. */
static void scribble(nsdk_canvas *c)
{
    size_t n = (size_t)c->w * (size_t)c->h;
    size_t i;

    for (i = 0; i < n; i++) {
        uint32_t v = next();

        if ((v & 7u) == 0u) {
            v |= 0xff000000u;
        }
        c->px[i] = v;
    }
}

static void test_buffer(void)
{
    static const int sizes[][2] = {
        {1, 1}, {7, 5}, {64, 48}, {0, 4}, {4, 0}, {-3, 4}, {4, -3},
        {65536, 65536}, {INT_MAX, 2}, {INT_MAX, INT_MAX},
    };
    uint32_t mem[12];
    nsdk_canvas c;
    size_t i;

    fold_start();
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        bool made;

        /* A refused init must still empty whatever the struct held. */
        memset(&c, 0x5a, sizeof c);
        made = nsdk_canvas_init(&c, sizes[i][0], sizes[i][1]);
        /* Only the first three sizes can be made. */
        expect(made == (i < 3), "init makes a canvas of a good size and no other");
        fold(made);
        fold_struct(&c);
        if (made) {
            fold_pixels(&c);
        } else {
            expect(c.px == NULL && c.w == 0 && c.h == 0 && c.clip_x1 == 0 && c.clip_y1 == 0 && !c.owns_px,
                   "a refused init leaves the canvas empty");
        }
        nsdk_canvas_free(&c);
        expect(c.px == NULL && c.w == 0 && c.h == 0 && c.clip_x1 == 0 && c.clip_y1 == 0 && !c.owns_px,
               "free leaves it empty");
    }
    expect(!nsdk_canvas_init(NULL, 4, 4), "init of no canvas is refused");
    expect(nsdk_canvas_init(&c, 7, 5) && c.px != NULL && c.w == 7 && c.h == 5 && c.owns_px &&
           c.clip_x0 == 0 && c.clip_y0 == 0 && c.clip_x1 == 7 && c.clip_y1 == 5 && c.px[0] == 0u && c.px[34] == 0u,
           "init gives a zeroed 7 x 5 with the whole clip");
    nsdk_canvas_free(&c);
    expect(!nsdk_canvas_init(&c, 0, 5) && c.px == NULL && c.w == 0, "a zero width gives nothing");

    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        memset(&c, 0x5a, sizeof c);
        nsdk_canvas_wrap(&c, mem, sizes[i][0], sizes[i][1]);
        expect((c.px == mem) == (i < 3) && (c.px == mem || c.px == NULL), "wrap takes a good size and no other");
        fold_struct(&c);
    }
    nsdk_canvas_wrap(&c, NULL, 4, 3);
    expect(c.px == NULL && c.w == 0 && c.h == 0, "wrap of no memory gives nothing");
    nsdk_canvas_wrap(&c, mem, 4, 3);
    expect(c.px == mem && c.w == 4 && c.h == 3 && !c.owns_px && c.clip_x0 == 0 && c.clip_y0 == 0 &&
           c.clip_x1 == 4 && c.clip_y1 == 3, "wrap takes the memory and does not own it");
    mem[0] = 0x12345678u;
    nsdk_canvas_free(&c);
    expect(c.px == NULL && mem[0] == 0x12345678u, "free leaves wrapped memory alone");
    nsdk_canvas_free(NULL);
    nsdk_canvas_wrap(NULL, mem, 4, 3);
    pin("buffer");
}

#if defined(__GLIBC__) && __GLIBC_PREREQ(2, 33)
#define CAN_COUNT_MEMORY 1
/* Bytes the allocator holds for the program, mapped blocks included. */
static size_t held(void)
{
    struct mallinfo2 info = mallinfo2();

    return info.uordblks + info.hblkhd;
}
#endif

/* A canvas that init made gives its buffer back when it is freed. */
static void test_free_gives_the_buffer_back(void)
{
#ifdef CAN_COUNT_MEMORY
    enum { SIDE = 1024 };
    size_t before = held();
    size_t with;
    nsdk_canvas c;

    expect(nsdk_canvas_init(&c, SIDE, SIDE), "a canvas to free");
    with = held();
    expect(with >= before + (size_t)SIDE * SIDE * 4u, "the buffer is on the heap");
    nsdk_canvas_free(&c);
    expect(held() == before, "free gives the buffer back");
#endif
}

static int clip_number(void)
{
    static const int edge[] = {0, 1, -1, 5, 31, 32, 33, 47, 48, 49, 1000, -1000,
                               INT_MAX, INT_MIN, INT_MAX - 1, INT_MIN + 1};
    uint32_t v = next();

    return (v & 3u) == 0u ? edge[(v >> 8) % (sizeof edge / sizeof edge[0])]
                          : (int)((v >> 8) % 80u) - 16;
}

static void test_clip(void)
{
    nsdk_canvas c;
    int i;

    expect(nsdk_canvas_init(&c, 48, 32), "clip canvas");
    fold_start();
    for (i = 0; i < 20000; i++) {
        int x = clip_number();
        int y = clip_number();
        int w = clip_number();
        int h = clip_number();

        nsdk_canvas_set_clip(&c, x, y, w, h);
        fold_struct(&c);
        expect(c.clip_x0 >= 0 && c.clip_x0 <= c.clip_x1 && c.clip_x1 <= 48 &&
               c.clip_y0 >= 0 && c.clip_y0 <= c.clip_y1 && c.clip_y1 <= 32, "the clip stays inside");
    }
    pin("clip");
    nsdk_canvas_set_clip(&c, 5, 6, 10, 4);
    expect(c.clip_x0 == 5 && c.clip_y0 == 6 && c.clip_x1 == 15 && c.clip_y1 == 10, "clip 5,6 10x4");
    nsdk_canvas_set_clip(&c, -4, 30, 10, 10);
    expect(c.clip_x0 == 0 && c.clip_y0 == 30 && c.clip_x1 == 6 && c.clip_y1 == 32, "clip cut at the sides");
    nsdk_canvas_set_clip(&c, 10, 10, 0, 5);
    expect(c.clip_x0 == 10 && c.clip_x1 == 10 && c.clip_y0 == 10 && c.clip_y1 == 15, "an empty clip is empty");
    nsdk_canvas_set_clip(&c, 10, 10, 5, -1);
    expect(c.clip_x0 == 10 && c.clip_x1 == 15 && c.clip_y0 == 10 && c.clip_y1 == 10, "a negative height is empty");
    nsdk_canvas_set_clip(&c, 60, 40, 5, 5);
    expect(c.clip_x0 == 48 && c.clip_x1 == 48 && c.clip_y0 == 32 && c.clip_y1 == 32, "a clip past the canvas is empty at its corner");
    nsdk_canvas_set_clip(&c, INT_MAX, INT_MIN, INT_MAX, INT_MAX);
    expect(c.clip_x0 == 48 && c.clip_x1 == 48 && c.clip_y0 == 0 && c.clip_y1 == 0, "the largest numbers do not overflow");
    nsdk_canvas_reset_clip(&c);
    expect(c.clip_x0 == 0 && c.clip_y0 == 0 && c.clip_x1 == 48 && c.clip_y1 == 32, "reset gives the whole canvas");
    nsdk_canvas_set_clip(NULL, 0, 0, 1, 1);
    nsdk_canvas_reset_clip(NULL);
    nsdk_canvas_free(&c);
}

static void test_clear(void)
{
    static const int sizes[][2] = {{1, 1}, {7, 1}, {8, 1}, {9, 1}, {5, 3}, {64, 48}};
    nsdk_canvas c;
    size_t i;
    size_t at;

    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        uint32_t rgb = next();
        uint32_t want = 0xff000000u | (rgb & 0xffffffu);
        int wrong = 0;

        expect(nsdk_canvas_init(&c, sizes[i][0], sizes[i][1]), "clear canvas");
        scribble(&c);
        /* The clear is of the whole frame, whatever the clip. */
        nsdk_canvas_set_clip(&c, 1, 0, 2, 1);
        nsdk_canvas_clear(&c, rgb);
        for (at = 0; at < (size_t)c.w * (size_t)c.h; at++) {
            wrong += c.px[at] != want;
        }
        expect(wrong == 0, "clear stores the opaque colour in every pixel, whatever the clip");
        nsdk_canvas_free(&c);
    }
    expect(nsdk_canvas_init(&c, 2, 2), "clear canvas");
    nsdk_canvas_clear(&c, 0xab123456u);
    expect(c.px[0] == 0xff123456u && c.px[3] == 0xff123456u, "the colour's top byte is not an alpha");
    nsdk_canvas_free(&c);
    nsdk_canvas_clear(NULL, 0u);
    memset(&c, 0, sizeof c);
    nsdk_canvas_clear(&c, 0u);
}

/* A coordinate on or near a canvas side of that many pixels: whole, on a
 * half, or anywhere to a thousandth. */
static float coordinate(int span)
{
    uint32_t v = next();
    int reach = span + span / 4 + 4;
    int at = (int)((v >> 8) % (uint32_t)reach) - span / 8 - 2;

    switch (v & 7u) {
    case 0:
    case 1:
    case 2:
        return (float)at;
    case 3:
        return (float)at + 0.5f;
    default:
        return (float)at + (float)((v >> 20) % 1000u) / 1000.0f;
    }
}

/* Values a fill must refuse or cut down without overflow. */
static float awkward(void)
{
    static const float odd[] = {0.0f, -0.0f, 1e9f, -1e9f, 3.0e38f, -3.0e38f, 16777216.0f,
                                16777217.0f, 33554432.0f, 1e-30f, -7.25f, 2147483648.0f};
    uint32_t v = next();

    switch (v & 7u) {
    case 0:
        return NAN;
    case 1:
        return (v & 256u) ? INFINITY : -INFINITY;
    default:
        return odd[(v >> 8) % (sizeof odd / sizeof odd[0])];
    }
}

static float opacity(void)
{
    static const float odd[] = {1.0f, 1.0f, 1.0f, 1.5f, 0.001f, 0.0019f, 0.002f, 0.5f, 0.999f,
                                0.9999f, 0.88f, 0.95f, 255.0f / 256.0f, 0.0f, -1.0f};
    uint32_t v = next();

    switch (v & 15u) {
    case 0:
        return (v & 256u) ? NAN : INFINITY;
    case 1:
    case 2:
    case 3:
    case 4:
        return odd[(v >> 8) % (sizeof odd / sizeof odd[0])];
    default:
        return (float)((v >> 8) % 1001u) / 1000.0f;
    }
}

static void test_fill(void)
{
    static const int sizes[][2] = {{64, 48}, {33, 17}, {64, 48}, {7, 5}, {3, 80}, {1, 1}};
    static const char *const names[] = {"fill 64x48", "fill 33x17", "fill 64x48 again", "fill 7x5",
                                        "fill 3x80", "fill 1x1"};
    enum { EACH = 10000 };
    nsdk_canvas c;
    size_t size;
    int i;
    long touched = 0;
    long refused = 0;

    for (size = 0; size < sizeof sizes / sizeof sizes[0]; size++) {
        expect(nsdk_canvas_init(&c, sizes[size][0], sizes[size][1]), "fill canvas");
        scribble(&c);
        fold_start();
        for (i = 0; i < EACH; i++) {
            float v[4];
            float alpha = opacity();
            uint32_t rgb = next();
            uint32_t pick = next();
            uint32_t before;
            uint32_t after;

            v[0] = coordinate(c.w);
            v[1] = coordinate(c.h);
            v[2] = fabsf(coordinate(c.w)) + 0.25f;
            v[3] = fabsf(coordinate(c.h)) + 0.25f;
            if ((pick & 7u) == 0u) {
                v[(pick >> 4) & 3u] = awkward();
            } else if ((pick & 7u) == 1u) {
                v[2 + ((pick >> 4) & 1u)] *= -1.0f;
            }
            if ((i & 63) == 0) {
                scribble(&c);
                nsdk_canvas_reset_clip(&c);
            } else if ((i & 63) == 24) {
                /* A clip inside the canvas. */
                int cx = (int)(next() % (uint32_t)c.w);
                int cy = (int)(next() % (uint32_t)c.h);
                int cw = 1 + (int)(next() % (uint32_t)c.w);
                int ch = 1 + (int)(next() % (uint32_t)c.h);

                nsdk_canvas_set_clip(&c, cx, cy, cw, ch);
            } else if ((i & 63) == 48) {
                /* Any clip at all, for a few fills. */
                int cx = clip_number();
                int cy = clip_number();
                int cw = clip_number();
                int ch = clip_number();

                nsdk_canvas_set_clip(&c, cx, cy, cw, ch);
            } else if ((i & 63) == 56) {
                nsdk_canvas_reset_clip(&c);
            }
            before = sum(&c);
            nsdk_canvas_fill_rect(&c, v[0], v[1], v[2], v[3], rgb, alpha);
            after = sum(&c);
            /* Every fill's result goes into the sum, so one wrong pixel in
             * one fill shows even if a later fill paints over it. */
            fold(after);
            if (before != after) {
                touched++;
            } else {
                refused++;
            }
        }
        fold_pixels(&c);
        pin(names[size]);
        nsdk_canvas_free(&c);
    }
    /* Many of the fills must reach the pixels, or a matching sum means
     * little; and some must be turned away, or refusal is not covered. */
    if (touched < 15000 || refused < 3000) {
        fprintf(stderr, "%ld fills changed pixels and %ld did not\n", touched, refused);
    }
    expect(touched >= 15000, "at least 15000 of the 60000 fills changed pixels");
    expect(refused >= 3000, "some fills were refused or fell outside");
    nsdk_canvas_fill_rect(NULL, 0, 0, 1, 1, 0u, 1.0f);
}

/* Results by number, worked out by hand from the mix:
 * channel += ((colour - channel) * steps) >> 8, steps = alpha * 256 rounded. */
static void test_fill_numbers(void)
{
    nsdk_canvas c;

    expect(nsdk_canvas_init(&c, 6, 3), "numbers canvas");
    nsdk_canvas_clear(&c, 0x000000u);
    nsdk_canvas_fill_rect(&c, 1.0f, 1.0f, 2.0f, 1.0f, 0x336699u, 1.0f);
    expect(c.px[6] == 0xff000000u && c.px[7] == 0xff336699u && c.px[8] == 0xff336699u &&
           c.px[9] == 0xff000000u && c.px[1] == 0xff000000u && c.px[13] == 0xff000000u,
           "an opaque fill on whole pixels stores the colour and nothing beside it");
    nsdk_canvas_clear(&c, 0x000000u);
    nsdk_canvas_fill_rect(&c, 0.5f, 0.0f, 1.0f, 1.0f, 0xffffffu, 1.0f);
    expect(c.px[0] == 0xff7f7f7fu && c.px[1] == 0xff7f7f7fu && c.px[2] == 0xff000000u,
           "a fill across two half pixels gives each half the colour");
    nsdk_canvas_clear(&c, 0x000000u);
    nsdk_canvas_fill_rect(&c, 0.0f, 0.0f, 1.0f, 1.0f, 0xff8040u, 0.25f);
    expect(c.px[0] == 0xff3f2010u, "a quarter alpha moves each channel a quarter of the way");
    nsdk_canvas_clear(&c, 0xffffffu);
    nsdk_canvas_fill_rect(&c, 0.0f, 0.0f, 1.0f, 1.0f, 0x000000u, 0.5f);
    expect(c.px[0] == 0xff7f7f7fu, "half black on white rounds down");
    c.px[0] = 0x00000000u;
    nsdk_canvas_fill_rect(&c, 0.0f, 0.0f, 1.0f, 1.0f, 0xffffffu, 0.5f);
    expect(c.px[0] == 0x7f7f7f7fu, "the alpha channel is mixed toward opaque");
    c.px[0] = 0x00102030u;
    nsdk_canvas_fill_rect(&c, 0.0f, 0.0f, 1.0f, 1.0f, 0xabcdefu, 1.0f);
    expect(c.px[0] == 0xffabcdefu, "an opaque fill makes the pixel opaque");
    nsdk_canvas_clear(&c, 0x000000u);
    nsdk_canvas_fill_rect(&c, 0.0f, 0.0f, 1.0f, 1.0f, 0xffffffu, 0.0019f);
    expect(c.px[0] == 0xff000000u, "an alpha under half a step draws nothing");
    nsdk_canvas_fill_rect(&c, 0.0f, 0.0f, 1.0f, 1.0f, 0xffffffu, 0.002f);
    expect(c.px[0] == 0xff000000u, "one step moves a channel by 255 / 256, which is still zero");
    nsdk_canvas_fill_rect(&c, 0.0f, 0.0f, 1.0f, 1.0f, 0xffffffu, 0.008f);
    expect(c.px[0] == 0xff010101u, "two steps move 255 by one");
    nsdk_canvas_clear(&c, 0x000000u);
    nsdk_canvas_set_clip(&c, 2, 1, 2, 1);
    nsdk_canvas_fill_rect(&c, -10.0f, -10.0f, 100.0f, 100.0f, 0xffffffu, 1.0f);
    expect(c.px[7] == 0xff000000u && c.px[8] == 0xffffffffu && c.px[9] == 0xffffffffu &&
           c.px[10] == 0xff000000u && c.px[2] == 0xff000000u && c.px[14] == 0xff000000u,
           "a fill stays inside the clip");
    nsdk_canvas_free(&c);
}

static void test_pack(void)
{
    uint8_t bytes[4 * 35 + 4];
    nsdk_canvas c;
    size_t i;

    expect(nsdk_canvas_init(&c, 7, 5), "pack canvas");
    scribble(&c);
    memset(bytes, 0xee, sizeof bytes);
    expect(nsdk_canvas_pack_rgba(&c, bytes, 140) && bytes[140] == 0xee, "pack stops at the last pixel");
    fold_start();
    for (i = 0; i < 140; i++) {
        fold(bytes[i]);
    }
    pin("pack");
    c.px[0] = 0x11223344u;
    c.px[34] = 0xa1b2c3d4u;
    expect(nsdk_canvas_pack_rgba(&c, bytes, sizeof bytes) && bytes[0] == 0x22 && bytes[1] == 0x33 &&
           bytes[2] == 0x44 && bytes[3] == 0x11 && bytes[136] == 0xb2 && bytes[137] == 0xc3 &&
           bytes[138] == 0xd4 && bytes[139] == 0xa1, "bytes are red, green, blue, alpha");
    memset(bytes, 0xee, sizeof bytes);
    expect(!nsdk_canvas_pack_rgba(&c, bytes, 139) && bytes[0] == 0xee, "a short buffer is refused and left alone");
    expect(!nsdk_canvas_pack_rgba(&c, NULL, 140) && !nsdk_canvas_pack_rgba(NULL, bytes, 140),
           "pack needs a canvas and a buffer");
    nsdk_canvas_free(&c);
}

static long slurp(const char *path, unsigned char *into, long room)
{
    FILE *file = fopen(path, "rb");
    long got;

    if (file == NULL) {
        return -1;
    }
    got = (long)fread(into, 1, (size_t)room, file);
    fclose(file);
    return got;
}

static void test_ppm(void)
{
    /* 100 x 50 is 5000 pixels, so the writer's second block is used. */
    enum { W = 100, H = 50, ROOM = 15 + W * H * 3 + 8 };
    static unsigned char bytes[ROOM];
    const char *tmp = getenv("TMPDIR");
    char dir[200];
    char path[220];
    char missing[220];
    nsdk_canvas c;
    long n;
    long i;

    snprintf(dir, sizeof dir, "%.160s/nsdk-canvas-XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    expect(mkdtemp(dir) != NULL, "a scratch directory");
    snprintf(path, sizeof path, "%s/a.ppm", dir);
    snprintf(missing, sizeof missing, "%s/none/c.ppm", dir);
    expect(nsdk_canvas_init(&c, W, H), "ppm canvas");
    scribble(&c);
    c.px[0] = 0x80112233u;
    c.px[W * H - 1] = 0x00a0b0c0u;
    errno = EDOM;
    expect(nsdk_canvas_write_ppm(&c, path), "the file is written");
    expect(errno == 0, "errno is clear after a write");
    n = slurp(path, bytes, ROOM);
    expect(n == 14 + W * H * 3, "the file is its header and three bytes a pixel");
    expect(n > 17 && memcmp(bytes, "P6\n100 50\n255\n", 14) == 0 && bytes[14] == 0x11 && bytes[15] == 0x22 &&
           bytes[16] == 0x33 && bytes[n - 3] == 0xa0 && bytes[n - 2] == 0xb0 && bytes[n - 1] == 0xc0,
           "P6 header, then red, green, blue from the first pixel to the last");
    fold_start();
    for (i = 0; i < n; i++) {
        fold(bytes[i]);
    }
    pin("ppm");
    errno = 0;
    expect(!nsdk_canvas_write_ppm(&c, missing) && errno == ENOENT, "a missing directory fails with its reason");
    {
        /* A device with no room. The big frame fails while its blocks are
         * written; the 1 x 1 frame fits the stream's buffer, so only the
         * close can report it. */
        FILE *full = fopen("/dev/full", "wb");
        nsdk_canvas dot;

        if (full != NULL) {
            fclose(full);
            errno = 0;
            expect(!nsdk_canvas_write_ppm(&c, "/dev/full") && errno == ENOSPC,
                   "a full device fails the write with no space");
            expect(nsdk_canvas_init(&dot, 1, 1), "dot canvas");
            errno = 0;
            expect(!nsdk_canvas_write_ppm(&dot, "/dev/full") && errno == ENOSPC,
                   "a full device fails the close of a small file");
            nsdk_canvas_free(&dot);
        }
    }
    errno = 0;
    expect(!nsdk_canvas_write_ppm(&c, NULL) && errno == EINVAL, "no path is refused");
    errno = 0;
    expect(!nsdk_canvas_write_ppm(NULL, path) && errno == EINVAL, "no canvas is refused");
    unlink(path);
    rmdir(dir);
    nsdk_canvas_free(&c);
}

/* Past 2^24 a float no longer holds every pixel coordinate, and the fill
 * stops its whole-pixel run there. The pixels round 2^24 are pinned, along
 * a row and down a column. */
static void test_fill_past_float_range(void)
{
    enum { PAST = 8, LONG = (1 << 24) + PAST, FROM = (1 << 24) - 4 };
    uint32_t *mem = calloc((size_t)LONG, sizeof(uint32_t));
    int turn;

    if (mem == NULL) {
        /* No room for a 64 MB buffer is not a fault of the canvas. */
        return;
    }
    for (turn = 0; turn < 2; turn++) {
        nsdk_canvas c;
        int painted = 0;
        int i;

        memset(&c, 0, sizeof c);
        memset(mem, 0, (size_t)LONG * sizeof(uint32_t));
        nsdk_canvas_wrap(&c, mem, turn == 0 ? LONG : 1, turn == 0 ? 1 : LONG);
        expect(c.px == mem, "a canvas past 2^24 is wrapped");
        /* Opaque, then a half-covering colour over it from a fractional edge. */
        if (turn == 0) {
            nsdk_canvas_fill_rect(&c, (float)FROM, 0.0f, 12.0f, 1.0f, 0xffffffu, 1.0f);
            nsdk_canvas_fill_rect(&c, (float)FROM - 2.5f, 0.0f, 14.5f, 1.0f, 0x336699u, 0.5f);
        } else {
            nsdk_canvas_fill_rect(&c, 0.0f, (float)FROM, 1.0f, 12.0f, 0xffffffu, 1.0f);
            nsdk_canvas_fill_rect(&c, 0.0f, (float)FROM - 2.5f, 1.0f, 14.5f, 0x336699u, 0.5f);
        }
        fold_start();
        for (i = FROM - 8; i < LONG; i++) {
            fold(mem[i]);
            painted += i >= FROM && mem[i] != 0u;
        }
        for (i = 0; i < FROM - 8; i++) {
            if (mem[i] != 0u) {
                expect(0, "nothing is painted before the rectangle");
                break;
            }
        }
        pin(turn == 0 ? "row past 2^24" : "column past 2^24");
        expect(painted >= 4, "the fill reached 2^24");
    }
    free(mem);
}

int main(void)
{
    test_buffer();
    test_free_gives_the_buffer_back();
    test_clip();
    test_clear();
    test_fill();
    test_fill_numbers();
    test_fill_past_float_range();
    test_pack();
    test_ppm();
#ifdef NSDK_CANVAS_REFERENCE
    return failures ? 1 : 0;
#else
    if (failures) {
        fprintf(stderr, "test-canvas: %d failed\n", failures);
        return 1;
    }
    printf("test-canvas: ok\n");
    return 0;
#endif
}
