#include "amiterrain.h"

#include <stdio.h>

static int write_be16(FILE *f, uint16_t v)
{
    if (fputc((int)(v >> 8), f) == EOF) return -1;
    if (fputc((int)(v & 255), f) == EOF) return -1;
    return 0;
}

static int read_be16(FILE *f, uint16_t *v)
{
    int hi = fgetc(f), lo;
    if (hi == EOF) return -1;
    lo = fgetc(f);
    if (lo == EOF) return -1;
    *v = (uint16_t)(((uint16_t)hi << 8) | (uint16_t)lo);
    return 0;
}

int at_write_pgm16(const char *path, const ATTerrain *t)
{
    FILE *f;
    size_t i, n;
    if (!path || !t || !t->samples) return -1;
    f = fopen(path, "wb");
    if (!f) return -1;
    if (fprintf(f, "P5\n%lu %lu\n65535\n",
        (unsigned long)t->width, (unsigned long)t->height) < 0) { fclose(f); return -1; }
    n = (size_t)t->width * t->height;
    for (i = 0; i < n; ++i) if (write_be16(f, t->samples[i])) { fclose(f); return -1; }
    return fclose(f) == 0 ? 0 : -1;
}

int at_read_pgm16(const char *path, ATTerrain *t)
{
    FILE *f;
    unsigned long w, h, maxv;
    char magic[3];
    size_t i, n;
    if (!path || !t) return -1;
    f = fopen(path, "rb");
    if (!f) return -1;
    if (fscanf(f, "%2s", magic) != 1 || magic[0] != 'P' || magic[1] != '5' ||
        fscanf(f, "%lu %lu %lu", &w, &h, &maxv) != 3 || maxv != 65535UL ||
        w == 0 || h == 0 || w > 0xffffffffUL || h > 0xffffffffUL) { fclose(f); return -1; }
    if (fgetc(f) == EOF || at_terrain_init(t, (uint32_t)w, (uint32_t)h)) { fclose(f); return -1; }
    n = (size_t)t->width * t->height;
    for (i = 0; i < n; ++i) if (read_be16(f, &t->samples[i])) { at_terrain_free(t); fclose(f); return -1; }
    fclose(f);
    return 0;
}

int at_write_raw16be(const char *path, const ATTerrain *t)
{
    FILE *f;
    size_t i, n;
    if (!path || !t || !t->samples) return -1;
    f = fopen(path, "wb");
    if (!f) return -1;
    n = (size_t)t->width * t->height;
    for (i = 0; i < n; ++i) if (write_be16(f, t->samples[i])) { fclose(f); return -1; }
    return fclose(f) == 0 ? 0 : -1;
}

int at_read_raw16be(const char *path, uint32_t w, uint32_t h, ATTerrain *t)
{
    FILE *f;
    size_t i, n;
    if (!path || !t || at_terrain_init(t, w, h)) return -1;
    f = fopen(path, "rb");
    if (!f) { at_terrain_free(t); return -1; }
    n = (size_t)w * h;
    for (i = 0; i < n; ++i) if (read_be16(f, &t->samples[i])) { at_terrain_free(t); fclose(f); return -1; }
    fclose(f);
    return 0;
}
