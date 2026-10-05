#include "amiterrain.h"

#include <stdio.h>

int at_read_vistapro_binary(const char *path, uint32_t width, uint32_t height, ATTerrain *t)
{
    FILE *f;
    size_t x, y;
    if (!path || !t || at_terrain_init(t,width,height)) return -1;
    f=fopen(path,"rb");
    if (!f) { at_terrain_free(t); return -1; }

    /* VistaPro stream order is SW -> E, then rows progress north.
       AmiTerrain canonical memory order is north/top row first, so flip Y. */
    for (y=0;y<height;++y) {
        size_t dst_y=(size_t)height-1U-y;
        for (x=0;x<width;++x) {
            int hi=fgetc(f), lo=fgetc(f);
            int16_t signed_v;
            if (hi==EOF || lo==EOF) { at_terrain_free(t); fclose(f); return -1; }
            signed_v=(int16_t)(((uint16_t)hi<<8)|(uint16_t)lo);
            /* Preserve all signed 16-bit values losslessly by biasing into canonical u16. */
            t->samples[dst_y*(size_t)width+x]=(uint16_t)((int32_t)signed_v+32768);
        }
    }
    fclose(f);
    return 0;
}

int at_write_vistapro_binary(const char *path, const ATTerrain *t)
{
    FILE *f;
    size_t x, y;
    if (!path || !t || !t->samples) return -1;
    f=fopen(path,"wb");
    if (!f) return -1;
    for (y=0;y<t->height;++y) {
        size_t src_y=(size_t)t->height-1U-y;
        for (x=0;x<t->width;++x) {
            uint16_t u=t->samples[src_y*(size_t)t->width+x];
            int16_t s=(int16_t)((int32_t)u-32768);
            uint16_t bits=(uint16_t)s;
            if (fputc((int)(bits>>8),f)==EOF || fputc((int)(bits&255),f)==EOF) {
                fclose(f); return -1;
            }
        }
    }
    return fclose(f)==0 ? 0 : -1;
}
