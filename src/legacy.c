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


static uint32_t at_be32(const unsigned char *p)
{
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|(uint32_t)p[3];
}

static double at_be64f(const unsigned char *p)
{
    union { uint64_t u; double d; } x; int i;
    x.u=0; for(i=0;i<8;++i) x.u=(x.u<<8)|(uint64_t)p[i];
    return x.d;
}

static int at_put_be32(FILE *f, uint32_t v)
{
    return fputc((int)(v>>24),f)==EOF || fputc((int)((v>>16)&255),f)==EOF ||
           fputc((int)((v>>8)&255),f)==EOF || fputc((int)(v&255),f)==EOF ? -1:0;
}

static int at_put_be64f(FILE *f, double d)
{
    union { uint64_t u; double d; } x; int i; x.d=d;
    for(i=7;i>=0;--i) if(fputc((int)((x.u>>(i*8))&255U),f)==EOF) return -1;
    return 0;
}

static uint16_t at_be16(const unsigned char *p)
{
    return (uint16_t)(((uint16_t)p[0]<<8)|(uint16_t)p[1]);
}

int at_read_wcs_elev(const char *path, ATTerrain *t)
{
    FILE *f;
    unsigned char vbuf[4], hdr[64], sbuf[2];
    uint32_t vbits, rows, cols;
    float version;
    size_t count, i;

    if (!path || !t) return -1;
    f=fopen(path,"rb");
    if (!f) return -1;
    if (fread(vbuf,1,4,f)!=4) { fclose(f); return -1; }

    /* Original Amiga WCS writes native big-endian IEEE-754 float. */
    vbits=at_be32(vbuf);
    {
        union { uint32_t u; float f; } cvt;
        cvt.u=vbits; version=cvt.f;
    }
    if (!(version > 0.99f && version < 1.03f)) { fclose(f); return -1; }

    /* WCS 1.00 uses a 48-byte header; 1.01/1.02 use 64 bytes.
       rows and columns are the first two signed 32-bit fields. */
    {
        size_t hlen=(version < 1.005f) ? 48U : 64U;
        if (fread(hdr,1,hlen,f)!=hlen) { fclose(f); return -1; }
    }
    rows=at_be32(hdr);
    cols=at_be32(hdr+4);
    if (cols==0 || rows>=0x7fffffffU || cols>=0x7fffffffU) { fclose(f); return -1; }

    /* Amiga WCS stores rows as the last row index, hence rows + 1 samples high. */
    if (at_terrain_init(t,cols,rows+1U)) { fclose(f); return -1; }
    t->geo.origin_lat=at_be64f(hdr+8);
    t->geo.origin_lon=at_be64f(hdr+16);
    t->geo.step_lat=at_be64f(hdr+24);
    t->geo.step_lon=at_be64f(hdr+32);
    if (version >= 1.005f) {
        t->geo.elevation_scale=at_be64f(hdr+40);
        t->geo.valid=1;
    } else {
        t->geo.elevation_scale=1.0;
        t->geo.valid=1;
    }
    count=(size_t)t->width*(size_t)t->height;
    for (i=0;i<count;++i) {
        int16_t s;
        if (fread(sbuf,1,2,f)!=2) { at_terrain_free(t); fclose(f); return -1; }
        s=(int16_t)at_be16(sbuf);
        t->samples[i]=(uint16_t)((int32_t)s+32768);
    }
    fclose(f);
    return 0;
}


int at_write_wcs_elev(const char *path, const ATTerrain *t)
{
    FILE *f; size_t i,n; int16_t minv=32767,maxv=-32768;
    union { float f; uint32_t u; } ver;
    if(!path || !t || !t->samples || !t->width || !t->height) return -1;
    f=fopen(path,"wb"); if(!f) return -1;
    ver.f=1.02f;
    if(at_put_be32(f,ver.u) || at_put_be32(f,t->height-1U) || at_put_be32(f,t->width) ||
       at_put_be64f(f,t->geo.valid?t->geo.origin_lat:0.0) ||
       at_put_be64f(f,t->geo.valid?t->geo.origin_lon:0.0) ||
       at_put_be64f(f,t->geo.valid?t->geo.step_lat:0.0) ||
       at_put_be64f(f,t->geo.valid?t->geo.step_lon:0.0) ||
       at_put_be64f(f,t->geo.valid?t->geo.elevation_scale:1.0)) { fclose(f); return -1; }
    n=(size_t)t->width*t->height;
    for(i=0;i<n;++i) { int16_t s=(int16_t)((int32_t)t->samples[i]-32768); if(s<minv)minv=s; if(s>maxv)maxv=s; }
    if(fputc(((uint16_t)maxv)>>8,f)==EOF || fputc(((uint16_t)maxv)&255,f)==EOF ||
       fputc(((uint16_t)minv)>>8,f)==EOF || fputc(((uint16_t)minv)&255,f)==EOF ||
       at_put_be32(f,(uint32_t)n) || at_put_be32(f,0) || at_put_be32(f,0)) { fclose(f); return -1; }
    for(i=0;i<n;++i) {
        uint16_t u=(uint16_t)(int16_t)((int32_t)t->samples[i]-32768);
        if(fputc((int)(u>>8),f)==EOF || fputc((int)(u&255),f)==EOF) { fclose(f); return -1; }
    }
    return fclose(f)==0?0:-1;
}
