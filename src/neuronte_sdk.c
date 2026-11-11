/*
 * Cell surface over libneuronte. Drawing is libneuronte's; this file finds
 * the library and the weight file, names draw nets, and addresses cells.
 */
#include "neuronte_sdk.h"

#include "nsdk_defaults.h"

#include <dlfcn.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef NSDK_DEFAULT_LIB
#define NSDK_DEFAULT_LIB ""
#endif
#ifndef NSDK_DEFAULT_WEIGHTS
#define NSDK_DEFAULT_WEIGHTS ""
#endif

typedef struct neuronte neuronte;

typedef int (*nsdk_open_file_fn)(const char *path, neuronte **out);
typedef void (*nsdk_close_fn)(neuronte *n);
typedef int (*nsdk_nets_fn)(const neuronte *n);
typedef int (*nsdk_route_fn)(const neuronte *n, uint32_t scalar);
typedef size_t (*nsdk_utf8_decode_fn)(const char *s, size_t len, uint32_t *scalar);
typedef int (*nsdk_set_binary_fn)(neuronte *n, int on);
typedef int (*nsdk_draw_scalar_fn)(neuronte *n, uint32_t *pixels, int stride,
                                  int width, int height, int x, int y,
                                  int cell_w, int cell_h, uint32_t scalar,
                                  uint32_t color);

_Static_assert(sizeof(void *) == sizeof(void (*)(void)), "dlsym pointer width");

struct nsdk {
    void *library;
    neuronte *face;
    nsdk_close_fn close_face;
    nsdk_nets_fn nets;
    nsdk_route_fn route;
    nsdk_utf8_decode_fn utf8_decode;
    nsdk_set_binary_fn set_binary;
    nsdk_draw_scalar_fn draw_scalar;
};

/* Names for indexes 0..5 when the file has six draw nets. The gate inside
 * a bundle is not a draw net; neuronte_nets does not count it, and an index
 * outside 0 .. nets-1 is rejected. */
static const char *const k_net_names[] = {
    "ascii", "draw", "symbol", "accent", "pixel", "braille"
};

_Static_assert(sizeof k_net_names / sizeof k_net_names[0] == 6,
               "six draw-net names");

static int load_symbol(void *library, const char *name, void *slot)
{
    void *symbol = dlsym(library, name);

    if (!symbol)
        return NEURONTE_ERR_IO;
    memcpy(slot, &symbol, sizeof symbol);
    return NEURONTE_OK;
}

static int file_readable(const char *path)
{
    struct stat info;

    if (!path || !path[0])
        return 0;
    if (stat(path, &info) != 0)
        return 0;
    return S_ISREG(info.st_mode) ? 1 : 0;
}

static int join_path(char *out, size_t out_len, const char *root, const char *rel)
{
    size_t root_len = strlen(root);
    size_t rel_len = strlen(rel);
    int slash = root_len > 0 && root[root_len - 1] != '/';
    size_t extra = slash ? 1u : 0u;
    size_t total;

    if (root_len > SIZE_MAX - extra || rel_len > SIZE_MAX - 1u ||
        root_len + extra > SIZE_MAX - (rel_len + 1u))
        return 0;
    total = root_len + extra + rel_len + 1u;
    if (total > out_len)
        return 0;
    memcpy(out, root, root_len);
    if (slash)
        out[root_len] = '/';
    memcpy(out + root_len + extra, rel, rel_len + 1u);
    return 1;
}

static int copy_path(char *out, size_t out_len, const char *path)
{
    size_t len = strlen(path);

    if (len == 0 || len + 1u > out_len)
        return NEURONTE_ERR_ARG;
    memcpy(out, path, len + 1u);
    return NEURONTE_OK;
}

static int try_dlopen(const char *path, void **library)
{
    void *opened;

    if (!file_readable(path))
        return NEURONTE_ERR_IO;
    opened = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!opened)
        return NEURONTE_ERR_IO;
    *library = opened;
    return NEURONTE_OK;
}

/* NEURONTE_LIB is the library when it is set. Otherwise try the two
 * checkout roots, then the fallback the build recorded. */
static int open_library(void **library)
{
    const char *forced = getenv("NEURONTE_LIB");
    const char *home = getenv("NEURONTE_HOME");
    const char *dir = getenv("NEURONTE_DIR");
    char path[PATH_MAX];

    *library = NULL;
    if (forced && forced[0])
        return try_dlopen(forced, library);
    if (home && home[0] &&
        join_path(path, sizeof path, home, "build/libneuronte.so") &&
        try_dlopen(path, library) == NEURONTE_OK)
        return NEURONTE_OK;
    if (dir && dir[0] &&
        join_path(path, sizeof path, dir, "build/libneuronte.so") &&
        try_dlopen(path, library) == NEURONTE_OK)
        return NEURONTE_OK;
    if (NSDK_DEFAULT_LIB[0] && try_dlopen(NSDK_DEFAULT_LIB, library) == NEURONTE_OK)
        return NEURONTE_OK;
    return NEURONTE_ERR_IO;
}

/* A caller-supplied path and NEURONTE_WEIGHTS are authoritative. Otherwise
 * the same roots as the library, then the build-time fallback. */
static int resolve_weights(const char *requested, char *out, size_t out_len)
{
    const char *forced;
    const char *home;
    const char *dir;

    if (requested)
        return copy_path(out, out_len, requested);

    forced = getenv("NEURONTE_WEIGHTS");
    if (forced && forced[0])
        return copy_path(out, out_len, forced);

    home = getenv("NEURONTE_HOME");
    if (home && home[0] &&
        join_path(out, out_len, home, "weights/neuronte-split-v4.nrt") &&
        file_readable(out))
        return NEURONTE_OK;

    dir = getenv("NEURONTE_DIR");
    if (dir && dir[0] &&
        join_path(out, out_len, dir, "weights/neuronte-split-v4.nrt") &&
        file_readable(out))
        return NEURONTE_OK;

    if (NSDK_DEFAULT_WEIGHTS[0] &&
        copy_path(out, out_len, NSDK_DEFAULT_WEIGHTS) == NEURONTE_OK &&
        file_readable(out))
        return NEURONTE_OK;
    return NEURONTE_ERR_IO;
}

int nsdk_open(const char *weights_or_null, nsdk **out)
{
    nsdk *created;
    void *library = NULL;
    char weights[PATH_MAX];
    nsdk_open_file_fn open_file = NULL;
    int rc;

    if (!out)
        return NEURONTE_ERR_ARG;
    *out = NULL;

    rc = resolve_weights(weights_or_null, weights, sizeof weights);
    if (rc != NEURONTE_OK)
        return rc;

    created = calloc(1, sizeof *created);
    if (!created)
        return NEURONTE_ERR_NOMEM;

    rc = open_library(&library);
    if (rc != NEURONTE_OK)
        goto fail;
    created->library = library;

    if (load_symbol(library, "neuronte_open_file", &open_file) != NEURONTE_OK ||
        load_symbol(library, "neuronte_close", &created->close_face) != NEURONTE_OK ||
        load_symbol(library, "neuronte_nets", &created->nets) != NEURONTE_OK ||
        load_symbol(library, "neuronte_route", &created->route) != NEURONTE_OK ||
        load_symbol(library, "neuronte_utf8_decode", &created->utf8_decode) != NEURONTE_OK ||
        load_symbol(library, "neuronte_set_binary", &created->set_binary) != NEURONTE_OK ||
        load_symbol(library, "neuronte_draw_scalar", &created->draw_scalar) != NEURONTE_OK) {
        rc = NEURONTE_ERR_IO;
        goto fail;
    }

    rc = open_file(weights, &created->face);
    if (rc != NEURONTE_OK)
        goto fail;

    *out = created;
    return NEURONTE_OK;

fail:
    if (created->face && created->close_face)
        created->close_face(created->face);
    free(created);
    if (library)
        dlclose(library);
    return rc;
}

void nsdk_close(nsdk *s)
{
    if (!s)
        return;
    if (s->face && s->close_face)
        s->close_face(s->face);
    s->face = NULL;
    if (s->library)
        dlclose(s->library);
    s->library = NULL;
    free(s);
}

int nsdk_nets(const nsdk *s)
{
    if (!s)
        return 0;
    return s->nets(s->face);
}

int nsdk_route(const nsdk *s, uint32_t scalar)
{
    if (!s)
        return NEURONTE_ERR_ARG;
    return s->route(s->face, scalar);
}

int nsdk_net_name(const nsdk *s, int index, char *buf, size_t len)
{
    int nets;
    const char *name;
    char numbered[16];
    size_t name_len;
    int wrote;

    if (!s || !buf || len == 0 || index < 0)
        return NEURONTE_ERR_ARG;
    nets = s->nets(s->face);
    if (nets < 0)
        return nets;
    if (index >= nets)
        return NEURONTE_ERR_ARG;
    if (nets == 6) {
        name = k_net_names[index];
    } else {
        wrote = snprintf(numbered, sizeof numbered, "net%d", index);
        if (wrote < 0 || (size_t)wrote >= sizeof numbered)
            return NEURONTE_ERR_ARG;
        name = numbered;
    }
    name_len = strlen(name);
    if (name_len + 1u > len)
        return NEURONTE_ERR_ARG;
    memcpy(buf, name, name_len + 1u);
    return NEURONTE_OK;
}

int nsdk_set_binary(nsdk *s, int on)
{
    if (!s)
        return NEURONTE_ERR_ARG;
    return s->set_binary(s->face, on);
}

int nsdk_draw_scalar(nsdk *s, uint32_t *pixels, int stride, int width, int height,
                     int x, int y, int cell_w, int cell_h,
                     uint32_t scalar, uint32_t color)
{
    if (!s)
        return NEURONTE_ERR_ARG;
    return s->draw_scalar(s->face, pixels, stride, width, height, x, y,
                          cell_w, cell_h, scalar, color);
}

static int scaled_origin(int origin, int index, int cell, int *out)
{
    long long value;

    if (cell <= 0)
        return NEURONTE_ERR_ARG;
    value = (long long)origin + (long long)index * (long long)cell;
    if (value > INT_MAX || value < INT_MIN)
        return NEURONTE_ERR_ARG;
    *out = (int)value;
    return NEURONTE_OK;
}

int nsdk_grid_put(nsdk *s, uint32_t *pixels, int stride, int width, int height,
                  int origin_x, int origin_y, int cell_w, int cell_h,
                  int col, int row, uint32_t scalar, uint32_t color)
{
    int x, y, rc;

    rc = scaled_origin(origin_x, col, cell_w, &x);
    if (rc != NEURONTE_OK)
        return rc;
    rc = scaled_origin(origin_y, row, cell_h, &y);
    if (rc != NEURONTE_OK)
        return rc;
    return nsdk_draw_scalar(s, pixels, stride, width, height, x, y,
                            cell_w, cell_h, scalar, color);
}

int nsdk_grid_text(nsdk *s, uint32_t *pixels, int stride, int width, int height,
                   int origin_x, int origin_y, int cell_w, int cell_h,
                   int col, int row, const char *utf8, size_t len, uint32_t color)
{
    size_t offset = 0;
    int cells = 0;
    int cursor = col;

    if (!s || (!utf8 && len > 0) || cell_w <= 0 || cell_h <= 0)
        return NEURONTE_ERR_ARG;

    while (offset < len) {
        uint32_t scalar = 0;
        size_t step;
        int x, y, rc;

        if (cursor == INT_MAX)
            return NEURONTE_ERR_ARG;
        step = s->utf8_decode(utf8 + offset, len - offset, &scalar);
        if (step == 0)
            return NEURONTE_ERR_FORMAT;
        rc = scaled_origin(origin_x, cursor, cell_w, &x);
        if (rc != NEURONTE_OK)
            return rc;
        rc = scaled_origin(origin_y, row, cell_h, &y);
        if (rc != NEURONTE_OK)
            return rc;
        rc = nsdk_draw_scalar(s, pixels, stride, width, height, x, y,
                              cell_w, cell_h, scalar, color);
        if (rc != NEURONTE_OK)
            return rc;
        if (offset > SIZE_MAX - step)
            return NEURONTE_ERR_ARG;
        offset += step;
        if (cells == INT_MAX)
            return NEURONTE_ERR_ARG;
        cells++;
        cursor++;
    }
    return cells;
}

int nsdk_grid_fill(nsdk *s, uint32_t *pixels, int stride, int width, int height,
                   int origin_x, int origin_y, int cell_w, int cell_h,
                   int col, int row, int cols, int rows,
                   uint32_t scalar, uint32_t color)
{
    int r, c;

    if (!s || cols < 0 || rows < 0 || cell_w <= 0 || cell_h <= 0)
        return NEURONTE_ERR_ARG;
    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols; c++) {
            long long cc = (long long)col + c;
            long long rr = (long long)row + r;
            int rc;

            if (cc > INT_MAX || cc < INT_MIN || rr > INT_MAX || rr < INT_MIN)
                return NEURONTE_ERR_ARG;
            rc = nsdk_grid_put(s, pixels, stride, width, height,
                               origin_x, origin_y, cell_w, cell_h,
                               (int)cc, (int)rr, scalar, color);
            if (rc != NEURONTE_OK)
                return rc;
        }
    }
    return NEURONTE_OK;
}

int nsdk_rgb(const uint32_t *pixels, int width, int height, uint8_t *out, size_t out_len)
{
    size_t wide, high, count, i;

    if (!pixels || !out || width < 0 || height < 0)
        return NEURONTE_ERR_ARG;
    wide = (size_t)width;
    high = (size_t)height;
    if (wide != 0 && high > SIZE_MAX / wide)
        return NEURONTE_ERR_ARG;
    count = wide * high;
    if (count > SIZE_MAX / 3u || out_len < count * 3u)
        return NEURONTE_ERR_ARG;
    for (i = 0; i < count; i++) {
        uint32_t pixel = pixels[i];
        out[i * 3u] = (uint8_t)(pixel >> 16);
        out[i * 3u + 1u] = (uint8_t)(pixel >> 8);
        out[i * 3u + 2u] = (uint8_t)pixel;
    }
    return NEURONTE_OK;
}
