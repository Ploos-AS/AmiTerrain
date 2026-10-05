#include "amiterrain.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
    puts("AmiTerrain M0");
    puts("usage:");
    puts("  amiterrain generate WIDTH HEIGHT SEED OUTPUT.pgm");
    puts("  amiterrain info INPUT.pgm");
    puts("  amiterrain convert INPUT.pgm OUTPUT.raw");
    puts("  amiterrain validate INPUT.pgm");
}

int main(int argc, char **argv)
{
    ATTerrain t = {0,0,0};
    if (argc < 2) { usage(); return 2; }

    if (!strcmp(argv[1], "generate") && argc == 6) {
        uint32_t w = (uint32_t)strtoul(argv[2], 0, 0);
        uint32_t h = (uint32_t)strtoul(argv[3], 0, 0);
        uint32_t seed = (uint32_t)strtoul(argv[4], 0, 0);
        if (at_terrain_init(&t, w, h)) return 1;
        at_generate_noise(&t, seed);
        if (at_write_pgm16(argv[5], &t)) { at_terrain_free(&t); return 1; }
        printf("%lux%lu seed=%lu checksum=%08lx\n",
            (unsigned long)w, (unsigned long)h, (unsigned long)seed,
            (unsigned long)at_checksum(&t));
        at_terrain_free(&t);
        return 0;
    }

    if ((!strcmp(argv[1], "info") || !strcmp(argv[1], "validate")) && argc == 3) {
        if (at_read_pgm16(argv[2], &t)) return 1;
        printf("%lux%lu checksum=%08lx\n", (unsigned long)t.width,
            (unsigned long)t.height, (unsigned long)at_checksum(&t));
        at_terrain_free(&t);
        return 0;
    }

    if (!strcmp(argv[1], "convert") && argc == 4) {
        if (at_read_pgm16(argv[2], &t)) return 1;
        if (at_write_raw16be(argv[3], &t)) { at_terrain_free(&t); return 1; }
        at_terrain_free(&t);
        return 0;
    }

    usage();
    return 2;
}
