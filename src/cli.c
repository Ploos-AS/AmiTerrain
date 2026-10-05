#include "amiterrain.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
    puts("AmiTerrain M0");
    puts("usage:");
    puts("  amiterrain generate WIDTH HEIGHT SEED OUTPUT.pgm|OUTPUT.atf");
    puts("  amiterrain info INPUT.pgm|INPUT.atf");
    puts("  amiterrain convert INPUT.pgm|INPUT.atf OUTPUT.pgm|OUTPUT.atf|OUTPUT.raw");
    puts("  amiterrain validate INPUT.pgm|INPUT.atf");
}

static int suffix(const char *s, const char *ext)
{
    size_t a=strlen(s), b=strlen(ext);
    return a>=b && !strcmp(s+a-b,ext);
}

static int load(const char *path, ATTerrain *t)
{
    if (suffix(path,".atf")) return at_read_atf(path,t);
    return at_read_pgm16(path,t);
}

static int save(const char *path, const ATTerrain *t)
{
    if (suffix(path,".atf")) return at_write_atf(path,t);
    if (suffix(path,".raw")) return at_write_raw16be(path,t);
    return at_write_pgm16(path,t);
}

int main(int argc, char **argv)
{
    ATTerrain t={0,0,0};
    if (argc<2) { usage(); return 2; }

    if (!strcmp(argv[1],"generate") && argc==6) {
        uint32_t w=(uint32_t)strtoul(argv[2],0,0);
        uint32_t h=(uint32_t)strtoul(argv[3],0,0);
        uint32_t seed=(uint32_t)strtoul(argv[4],0,0);
        if (at_terrain_init(&t,w,h)) return 1;
        at_generate_noise(&t,seed);
        if (save(argv[5],&t)) { at_terrain_free(&t); return 1; }
        printf("%lux%lu seed=%lu checksum=%08lx\n",(unsigned long)w,(unsigned long)h,
            (unsigned long)seed,(unsigned long)at_checksum(&t));
        at_terrain_free(&t); return 0;
    }

    if ((!strcmp(argv[1],"info") || !strcmp(argv[1],"validate")) && argc==3) {
        if (load(argv[2],&t)) return 1;
        printf("%lux%lu checksum=%08lx\n",(unsigned long)t.width,(unsigned long)t.height,
            (unsigned long)at_checksum(&t));
        at_terrain_free(&t); return 0;
    }

    if (!strcmp(argv[1],"convert") && argc==4) {
        if (load(argv[2],&t)) return 1;
        if (save(argv[3],&t)) { at_terrain_free(&t); return 1; }
        at_terrain_free(&t); return 0;
    }

    usage(); return 2;
}
