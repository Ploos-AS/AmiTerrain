#include "amiterrain.h"

#include <stdlib.h>

int at_terrain_init(ATTerrain *terrain, uint32_t width, uint32_t height)
{
    size_t count;
    if (!terrain || width == 0 || height == 0) return -1;
    if ((size_t)width > ((size_t)-1) / (size_t)height) return -1;
    count = (size_t)width * (size_t)height;
    if (count > ((size_t)-1) / sizeof(uint16_t)) return -1;
    terrain->samples = (uint16_t *)malloc(count * sizeof(uint16_t));
    if (!terrain->samples) return -1;
    terrain->width = width;
    terrain->height = height;
    return 0;
}

void at_terrain_free(ATTerrain *terrain)
{
    if (!terrain) return;
    free(terrain->samples);
    terrain->samples = 0;
    terrain->width = terrain->height = 0;
}

static uint32_t at_xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    if (x == 0) x = 0x6d2b79f5UL;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

void at_generate_noise(ATTerrain *terrain, uint32_t seed)
{
    size_t i, count;
    uint32_t state = seed;
    if (!terrain || !terrain->samples) return;
    count = (size_t)terrain->width * (size_t)terrain->height;
    for (i = 0; i < count; ++i)
        terrain->samples[i] = (uint16_t)(at_xorshift32(&state) >> 16);
}

uint32_t at_checksum(const ATTerrain *terrain)
{
    size_t i, count;
    uint32_t h = 2166136261UL;
    if (!terrain || !terrain->samples) return 0;
    count = (size_t)terrain->width * (size_t)terrain->height;
    for (i = 0; i < count; ++i) {
        uint16_t v = terrain->samples[i];
        h ^= (uint8_t)(v >> 8); h *= 16777619UL;
        h ^= (uint8_t)v;        h *= 16777619UL;
    }
    return h;
}
