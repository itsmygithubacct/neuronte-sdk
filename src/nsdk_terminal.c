/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L
#include "neuronte_sdk_terminal.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAME_LEN 128
#define PATH_LEN 1024
#define MAX_FILES 72
#define MAX_CAND 512
#define MAX_NETS 128
#define MAX_RANGES 65536
#define BUNDLE_HEAD 28
#define ROLE_BASE 0
#define ROLE_SPLIT 1
#define ROLE_TERM 2
#define ROLE_EMOJI 3

#ifndef NSDK_TERMINAL_EXTRA_DIR
#define NSDK_TERMINAL_EXTRA_DIR ""
#endif

struct range_rec {
    uint32_t first, last;
};

struct term_file {
    char name[NAME_LEN];
    neuronte *face;
    int owned;
    int role;
    long version;
    struct range_rec *ranges;
    uint32_t nranges;
    int table;
};

struct nsdk_terminal {
    struct term_file files[MAX_FILES];
    int nfiles;
};

struct candidate {
    char name[NAME_LEN];
    char path[PATH_LEN];
    int role;
    long version;
    int versioned;
    int used;
};

static uint32_t rd32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static const char *base_name(const char *path)
{
    const char *s = strrchr(path, '/');
    return s != NULL ? s + 1 : path;
}

static int has_suffix(const char *s, const char *suffix)
{
    size_t a = strlen(s), b = strlen(suffix);
    return a > b && strcmp(s + a - b, suffix) == 0;
}

static long numbered_version(const char *name, const char *prefix)
{
    size_t n = strlen(prefix);
    char *end;
    long v;
    if (strncmp(name, prefix, n) != 0)
        return -1;
    v = strtol(name + n, &end, 10);
    if (end == name + n || strcmp(end, ".nrt") != 0 || v < 0)
        return -1;
    return v;
}

static int term_versioned(const char *name)
{
    const char *dot = strrchr(name, '.');
    const char *p;
    if (dot == NULL || strcmp(dot, ".nrt") != 0)
        return 0;
    p = dot;
    while (p > name && p[-1] >= '0' && p[-1] <= '9')
        p--;
    return p < dot && p >= name + 2 && p[-1] == 'v' && p[-2] == '-';
}

static int range_has(const struct term_file *f, uint32_t scalar)
{
    uint32_t lo = 0, hi;
    if (!f->table)
        return 0;
    hi = f->nranges;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (scalar < f->ranges[mid].first)
            hi = mid;
        else if (scalar > f->ranges[mid].last)
            lo = mid + 1;
        else
            return 1;
    }
    return 0;
}

static int claims_every_scalar(const struct range_rec *ranges, uint32_t n)
{
    static const uint32_t probe[] = {0x10FFFCu, 0x10FFFDu, 0x10FFFEu};
    int i, p;
    if (n == 0)
        return 1;
    for (p = 0; p < 3; p++) {
        int hit = 0;
        for (i = 0; i < (int)n; i++)
            if (probe[p] >= ranges[i].first && probe[p] <= ranges[i].last)
                hit = 1;
        if (!hit)
            return 0;
    }
    return 1;
}

/* Read a version-2 bundle's range table. 0 on a usable table, -1 otherwise.
 * *claims is 1 when the table is missing or covers the probe scalars. */
static int load_ranges(const char *path, struct range_rec **out, uint32_t *n_out, int *claims)
{
    FILE *f;
    unsigned char head[BUNDLE_HEAD];
    unsigned char *buf = NULL;
    uint32_t k, nr, i;
    size_t need, got;
    struct range_rec *ranges = NULL;

    *out = NULL;
    *n_out = 0;
    *claims = 1;
    f = fopen(path, "rb");
    if (f == NULL)
        return -1;
    if (fread(head, 1, BUNDLE_HEAD, f) != BUNDLE_HEAD || memcmp(head, "NEURONTE", 8) != 0 ||
        rd32(head + 8) != 2) {
        fclose(f);
        return -1;
    }
    k = rd32(head + 12);
    nr = rd32(head + 16);
    if (k < 1 || k > MAX_NETS || nr > MAX_RANGES) {
        fclose(f);
        return -1;
    }
    need = BUNDLE_HEAD + 4u * (size_t)k + 12u * (size_t)nr;
    buf = malloc(need);
    if (buf == NULL) {
        fclose(f);
        return -1;
    }
    memcpy(buf, head, BUNDLE_HEAD);
    got = fread(buf + BUNDLE_HEAD, 1, need - BUNDLE_HEAD, f);
    fclose(f);
    if (got != need - BUNDLE_HEAD) {
        free(buf);
        return -1;
    }
    if (nr > 0) {
        ranges = calloc(nr, sizeof *ranges);
        if (ranges == NULL) {
            free(buf);
            return -1;
        }
    }
    for (i = 0; i < nr; i++) {
        size_t off = BUNDLE_HEAD + 4u * (size_t)k + 12u * (size_t)i;
        struct range_rec *r = &ranges[i];
        r->first = rd32(buf + off);
        r->last = rd32(buf + off + 4);
        if (r->first > r->last || r->last > 0x1FFFFFu || rd32(buf + off + 8) >= k ||
            (i > 0 && r->first <= ranges[i - 1].last)) {
            free(ranges);
            free(buf);
            return -1;
        }
    }
    free(buf);
    *claims = claims_every_scalar(ranges, nr);
    *out = ranges;
    *n_out = nr;
    return 0;
}

static void drop_file(struct term_file *f)
{
    if (f->owned && f->face != NULL)
        neuronte_close(f->face);
    free(f->ranges);
    memset(f, 0, sizeof *f);
}

static int known_name(const struct candidate *c, int n, const char *name)
{
    int i;
    for (i = 0; i < n; i++)
        if (strcmp(c[i].name, name) == 0)
            return 1;
    return 0;
}

static void add_candidate(struct candidate *c, int *n, const char *name, const char *path,
                          const char *base)
{
    long split, emoji;
    int role, versioned = 0;
    if (*n >= MAX_CAND || strlen(name) >= NAME_LEN || strlen(path) >= PATH_LEN)
        return;
    if (base != NULL && strcmp(name, base) == 0)
        return;
    if (known_name(c, *n, name))
        return;
    split = numbered_version(name, "neuronte-split-v");
    emoji = numbered_version(name, "neuronte-emoji-v");
    if (split >= 0) {
        role = ROLE_SPLIT;
    } else if (emoji >= 0) {
        role = ROLE_EMOJI;
    } else if (strncmp(name, "neuronte-term-", 14) == 0 && has_suffix(name, ".nrt")) {
        role = ROLE_TERM;
        versioned = term_versioned(name);
    } else {
        return;
    }
    snprintf(c[*n].name, NAME_LEN, "%s", name);
    snprintf(c[*n].path, PATH_LEN, "%s", path);
    c[*n].role = role;
    c[*n].version = split >= 0 ? split : emoji;
    c[*n].versioned = versioned;
    (*n)++;
}

static int dir_seen(char seen[][PATH_LEN], int n, const char *dir)
{
    int i;
    for (i = 0; i < n; i++)
        if (strcmp(seen[i], dir) == 0)
            return 1;
    return 0;
}

static void scan_dir(struct candidate *c, int *n, const char *dir, const char *base,
                     char seen[][PATH_LEN], int *nseen)
{
    DIR *d;
    struct dirent *e;
    char path[PATH_LEN];
    if (dir == NULL || dir[0] == '\0' || strlen(dir) >= PATH_LEN)
        return;
    if (dir_seen(seen, *nseen, dir))
        return;
    if (*nseen < 16) {
        snprintf(seen[*nseen], PATH_LEN, "%s", dir);
        (*nseen)++;
    }
    d = opendir(dir);
    if (d == NULL)
        return;
    while ((e = readdir(d)) != NULL) {
        if (!has_suffix(e->d_name, ".nrt"))
            continue;
        if ((size_t)snprintf(path, sizeof path, "%s/%s", dir, e->d_name) >= sizeof path)
            continue;
        add_candidate(c, n, e->d_name, path, base);
    }
    closedir(d);
}

static void scan_env_dir(struct candidate *c, int *n, const char *env, const char *base,
                         char seen[][PATH_LEN], int *nseen, int parent_of_file)
{
    const char *v = getenv(env);
    char dir[PATH_LEN];
    if (v == NULL || v[0] == '\0')
        return;
    if (!parent_of_file) {
        scan_dir(c, n, v, base, seen, nseen);
        return;
    }
    {
        DIR *d = opendir(v);
        if (d != NULL) {
            closedir(d);
            scan_dir(c, n, v, base, seen, nseen);
            return;
        }
    }
    {
        const char *slash = strrchr(v, '/');
        size_t len = slash != NULL ? (size_t)(slash - v) : 0;
        if (len == 0 || len >= sizeof dir)
            return;
        memcpy(dir, v, len);
        dir[len] = '\0';
        scan_dir(c, n, dir, base, seen, nseen);
    }
}

static void consider(struct candidate *c, int *n, const char *base, const char *base_dir)
{
    char seen[16][PATH_LEN];
    int nseen = 0;
    const char *home;
    char dir[PATH_LEN];
    scan_dir(c, n, base_dir, base, seen, &nseen);
    scan_env_dir(c, n, "NEUROTERM_WEIGHTS", base, seen, &nseen, 0);
    scan_env_dir(c, n, "NEURONTE_WEIGHTS", base, seen, &nseen, 1);
    {
        const char *home_n = getenv("NEURONTE_HOME");
        if (home_n != NULL && home_n[0] != '\0' &&
            (size_t)snprintf(dir, sizeof dir, "%s/weights", home_n) < sizeof dir)
            scan_dir(c, n, dir, base, seen, &nseen);
    }
    home = getenv("HOME");
    if (home != NULL && home[0] != '\0' &&
        (size_t)snprintf(dir, sizeof dir, "%s/.local/gpu_terminal/sources/neuronte/weights", home) <
            sizeof dir)
        scan_dir(c, n, dir, base, seen, &nseen);
    scan_dir(c, n, "/usr/share/neuronte/weights", base, seen, &nseen);
    if (NSDK_TERMINAL_EXTRA_DIR[0] != '\0')
        scan_dir(c, n, NSDK_TERMINAL_EXTRA_DIR, base, seen, &nseen);
}

static int open_supplement(struct nsdk_terminal *t, const struct candidate *c);

static int better(const struct candidate *a, const struct candidate *b)
{
    if (a->role == ROLE_TERM) {
        if (a->versioned != b->versioned)
            return a->versioned > b->versioned;
        return strcmp(a->name, b->name) < 0;
    }
    if (a->version != b->version)
        return a->version > b->version;
    return strcmp(a->name, b->name) < 0;
}

static void open_role(nsdk_terminal *t, struct candidate *c, int n, int role)
{
    int i;
    for (;;) {
        int best = -1;
        for (i = 0; i < n; i++) {
            if (c[i].used || c[i].role != role)
                continue;
            if (best < 0 || better(&c[i], &c[best]))
                best = i;
        }
        if (best < 0)
            return;
        c[best].used = 1;
        open_supplement(t, &c[best]);
    }
}

static int open_supplement(struct nsdk_terminal *t, const struct candidate *c)
{
    struct term_file *f;
    struct range_rec *ranges = NULL;
    uint32_t nr = 0;
    int claims = 1;
    neuronte *face = NULL;
    if (t->nfiles >= MAX_FILES)
        return 0;
    if (load_ranges(c->path, &ranges, &nr, &claims) != 0 || claims) {
        free(ranges);
        return 0;
    }
    if (neuronte_open_file(c->path, &face) != NEURONTE_OK) {
        free(ranges);
        return 0;
    }
#ifdef NSDK_HAVE_UNTRAINED
    neuronte_set_untrained(face, 1);
#endif
    f = &t->files[t->nfiles++];
    memset(f, 0, sizeof *f);
    snprintf(f->name, NAME_LEN, "%s", c->name);
    f->face = face;
    f->owned = 1;
    f->role = c->role;
    f->version = c->version;
    f->ranges = ranges;
    f->nranges = nr;
    f->table = 1;
    return 1;
}

static const struct term_file *scalar_file(const nsdk_terminal *t, uint32_t scalar)
{
    int i;
    if (t == NULL || scalar > 0x10FFFFu)
        return NULL;
    for (i = 0; i < t->nfiles; i++)
        if (range_has(&t->files[i], scalar))
            return &t->files[i];
    return NULL;
}

static const struct term_file *half_file(const nsdk_terminal *t, uint32_t scalar)
{
    int pass, i;
    uint32_t right;
    if (t == NULL || scalar > 0xEFFFFu)
        return NULL;
    right = 0x110000u + scalar;
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < t->nfiles; i++) {
            const struct term_file *f = &t->files[i];
            int emoji = f->role == ROLE_EMOJI;
            if ((pass == 0) != emoji)
                continue;
            if (range_has(f, scalar) && range_has(f, right))
                return f;
        }
    }
    return NULL;
}

int nsdk_terminal_open(neuronte *base, const char *base_path, nsdk_terminal **out)
{
    nsdk_terminal *t;
    struct candidate *cands;
    struct term_file *slot;
    char dir[PATH_LEN];
    const char *name = "";
    int nc = 0, i;
    struct range_rec *ranges = NULL;
    uint32_t nr = 0;
    int claims = 1;

    if (out == NULL)
        return NEURONTE_ERR_ARG;
    *out = NULL;
    if (base == NULL)
        return NEURONTE_ERR_ARG;
#ifdef NSDK_HAVE_UNTRAINED
    if (neuronte_set_untrained(base, 1) != NEURONTE_OK)
        return NEURONTE_ERR_ARG;
#endif
    t = calloc(1, sizeof *t);
    cands = calloc(MAX_CAND, sizeof *cands);
    if (t == NULL || cands == NULL) {
        free(t);
        free(cands);
        return NEURONTE_ERR_NOMEM;
    }
    if (base_path != NULL && base_path[0] != '\0') {
        const char *slash = strrchr(base_path, '/');
        name = base_name(base_path);
        if (load_ranges(base_path, &ranges, &nr, &claims) != 0) {
            free(ranges);
            ranges = NULL;
            nr = 0;
            claims = 1;
        }
        if (slash != NULL && slash > base_path) {
            size_t len = (size_t)(slash - base_path);
            if (len < sizeof dir) {
                memcpy(dir, base_path, len);
                dir[len] = '\0';
                consider(cands, &nc, name, dir);
            } else {
                consider(cands, &nc, name, NULL);
            }
        } else {
            consider(cands, &nc, name, NULL);
        }
    } else {
        consider(cands, &nc, NULL, NULL);
    }
    slot = &t->files[t->nfiles++];
    snprintf(slot->name, NAME_LEN, "%s", name[0] ? name : "base");
    slot->face = base;
    slot->owned = 0;
    slot->role = ROLE_BASE;
    slot->ranges = claims ? NULL : ranges;
    if (claims)
        free(ranges);
    else {
        slot->nranges = nr;
        slot->table = nr > 0;
    }
    /* Emoji and splits are few. Open them before term bundles so a long
     * term list cannot crowd the folder out of the cap. */
    open_role(t, cands, nc, ROLE_SPLIT);
    open_role(t, cands, nc, ROLE_EMOJI);
    open_role(t, cands, nc, ROLE_TERM);
    /* Lookup walks the array from the base. Term bundles belong before
     * emoji so a monochrome play triangle wins over the colour picture. */
    {
        struct term_file ordered[MAX_FILES];
        int n = 0, r;
        ordered[n++] = t->files[0];
        for (r = ROLE_SPLIT; r <= ROLE_EMOJI; r++)
            for (i = 1; i < t->nfiles; i++)
                if (t->files[i].role == r)
                    ordered[n++] = t->files[i];
        memcpy(t->files, ordered, (size_t)n * sizeof ordered[0]);
        t->nfiles = n;
    }
    free(cands);
    *out = t;
    return NEURONTE_OK;
}

void nsdk_terminal_close(nsdk_terminal *t)
{
    int i;
    if (t == NULL)
        return;
    for (i = 0; i < t->nfiles; i++)
        drop_file(&t->files[i]);
    free(t);
}

int nsdk_terminal_trained(const nsdk_terminal *t, uint32_t scalar)
{
    if (half_file(t, scalar) != NULL || scalar_file(t, scalar) != NULL)
        return 1;
    return 0;
}

int nsdk_terminal_draw(nsdk_terminal *t, uint32_t *pixels, int stride, int width, int height,
                       int x, int y, int cell_w, int cell_h, int span, uint32_t scalar,
                       uint32_t color, unsigned style)
{
    const struct term_file *f;
    neuronte *face;
    if (t == NULL || t->nfiles < 1 || pixels == NULL || cell_w < 1 || cell_h < 1 ||
        (span != 1 && span != 2))
        return NEURONTE_ERR_ARG;
    if (span == 2) {
        f = half_file(t, scalar);
        if (f != NULL) {
            int rc = neuronte_draw_half(f->face, pixels, stride, width, height, x, y, cell_w,
                                        cell_h, scalar, 0, color);
            if (rc != NEURONTE_OK)
                return rc;
            return neuronte_draw_half(f->face, pixels, stride, width, height, x + cell_w, y,
                                      cell_w, cell_h, scalar, 1, color);
        }
    }
    f = scalar_file(t, scalar);
    face = f != NULL ? f->face : t->files[0].face;
    return neuronte_draw_scalar_styled(face, pixels, stride, width, height, x, y, cell_w, cell_h,
                                       scalar, color, style);
}
