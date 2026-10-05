#ifndef AMITERRAIN_H
#define AMITERRAIN_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t width;
    uint32_t height;
    uint16_t *samples;
} ATTerrain;

int at_terrain_init(ATTerrain *terrain, uint32_t width, uint32_t height);
void at_terrain_free(ATTerrain *terrain);
void at_generate_noise(ATTerrain *terrain, uint32_t seed);
uint32_t at_checksum(const ATTerrain *terrain);

int at_write_pgm16(const char *path, const ATTerrain *terrain);
int at_read_pgm16(const char *path, ATTerrain *terrain);
int at_write_raw16be(const char *path, const ATTerrain *terrain);
int at_read_raw16be(const char *path, uint32_t width, uint32_t height, ATTerrain *terrain);
int at_write_atf(const char *path, const ATTerrain *terrain);
int at_read_atf(const char *path, ATTerrain *terrain);
int at_read_vistapro_binary(const char *path, uint32_t width, uint32_t height, ATTerrain *terrain);
int at_write_vistapro_binary(const char *path, const ATTerrain *terrain);

#endif
