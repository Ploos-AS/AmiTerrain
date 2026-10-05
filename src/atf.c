#include "amiterrain.h"

#include <stdio.h>
#include <string.h>

static int put_u32(FILE *f, uint32_t v)
{
    return fputc((int)(v >> 24), f) == EOF ||
           fputc((int)((v >> 16) & 255), f) == EOF ||
           fputc((int)((v >> 8) & 255), f) == EOF ||
           fputc((int)(v & 255), f) == EOF ? -1 : 0;
}

static int get_u32(FILE *f, uint32_t *v)
{
    int a=fgetc(f), b=fgetc(f), c=fgetc(f), d=fgetc(f);
    if (a==EOF || b==EOF || c==EOF || d==EOF) return -1;
    *v=((uint32_t)a<<24)|((uint32_t)b<<16)|((uint32_t)c<<8)|(uint32_t)d;
    return 0;
}

static int put_u16(FILE *f, uint16_t v)
{
    return fputc((int)(v >> 8), f) == EOF || fputc((int)(v & 255), f) == EOF ? -1 : 0;
}

static int get_u16(FILE *f, uint16_t *v)
{
    int a=fgetc(f), b=fgetc(f);
    if (a==EOF || b==EOF) return -1;
    *v=(uint16_t)(((uint16_t)a<<8)|(uint16_t)b);
    return 0;
}

static int put_f64(FILE *f, double d)
{
    union { double d; uint64_t u; } x; int i;
    x.d=d;
    for(i=7;i>=0;--i) if(fputc((int)((x.u>>(i*8))&255U),f)==EOF) return -1;
    return 0;
}

static int get_f64(FILE *f, double *d)
{
    union { double d; uint64_t u; } x; int i,c;
    x.u=0;
    for(i=7;i>=0;--i) { c=fgetc(f); if(c==EOF) return -1; x.u|=(uint64_t)(unsigned)c<<(i*8); }
    *d=x.d; return 0;
}

static int chunk(FILE *f, const char id[4], uint32_t size)
{
    return fwrite(id,1,4,f)==4 && put_u32(f,size)==0 ? 0 : -1;
}

int at_write_atf(const char *path, const ATTerrain *t)
{
    FILE *f;
    size_t i, n;
    uint32_t hmap_size, form_size;
    if (!path || !t || !t->samples) return -1;
    n=(size_t)t->width*(size_t)t->height;
    if (n > 0x7fffffffUL) return -1;
    hmap_size=(uint32_t)(n*2U);
    /* ATFN + HEAD(8+8) + SIZE(8+8) + HMAP(8+data). */
    form_size=4U+16U+16U+8U+hmap_size+(t->geo.valid ? 56U : 0U);
    f=fopen(path,"wb");
    if (!f) return -1;
    if (fwrite("FORM",1,4,f)!=4 || put_u32(f,form_size) ||
        fwrite("ATFN",1,4,f)!=4 ||
        chunk(f,"HEAD",8) || put_u16(f,0) || put_u16(f,1) || put_u32(f,0) ||
        chunk(f,"SIZE",8) || put_u32(f,t->width) || put_u32(f,t->height) ||
        (t->geo.valid && (chunk(f,"GEO ",48) || put_f64(f,t->geo.origin_lat) ||
         put_f64(f,t->geo.origin_lon) || put_f64(f,t->geo.step_lat) ||
         put_f64(f,t->geo.step_lon) || put_f64(f,t->geo.elevation_scale) ||
         put_u32(f,0) || put_u32(f,0))) ||
        chunk(f,"HMAP",hmap_size)) { fclose(f); return -1; }
    for (i=0;i<n;++i) if (put_u16(f,t->samples[i])) { fclose(f); return -1; }
    return fclose(f)==0 ? 0 : -1;
}

int at_read_atf(const char *path, ATTerrain *t)
{
    FILE *f;
    char id[4], type[4];
    uint32_t form_size, size, width=0, height=0;
    int have_head=0, have_size=0, have_hmap=0, have_geo=0;
    ATGeoMetadata geo={0};
    if (!path || !t) return -1;
    f=fopen(path,"rb");
    if (!f) return -1;
    if (fread(id,1,4,f)!=4 || memcmp(id,"FORM",4) || get_u32(f,&form_size) ||
        fread(type,1,4,f)!=4 || memcmp(type,"ATFN",4)) { fclose(f); return -1; }
    (void)form_size;
    while (fread(id,1,4,f)==4) {
        long pos;
        if (get_u32(f,&size)) { fclose(f); return -1; }
        pos=ftell(f);
        if (pos < 0) { fclose(f); return -1; }
        if (!memcmp(id,"HEAD",4)) {
            uint16_t major, minor;
            uint32_t flags;
            if (size < 8 || get_u16(f,&major) || get_u16(f,&minor) || get_u32(f,&flags) ||
                major != 0 || minor != 1) { fclose(f); return -1; }
            (void)flags; have_head=1;
        } else if (!memcmp(id,"SIZE",4)) {
            if (size < 8 || get_u32(f,&width) || get_u32(f,&height) || !width || !height) {
                fclose(f); return -1;
            }
            have_size=1;
        } else if (!memcmp(id,"GEO ",4)) {
            if (size < 48 || get_f64(f,&t->geo.origin_lat) || get_f64(f,&t->geo.origin_lon) ||
                get_f64(f,&t->geo.step_lat) || get_f64(f,&t->geo.step_lon) ||
                get_f64(f,&t->geo.elevation_scale)) { fclose(f); return -1; }
            t->geo.valid=1;
        } else if (!memcmp(id,"HMAP",4)) {
            size_t i,n;
            if (!have_size || (size_t)width > ((size_t)-1)/(size_t)height) { fclose(f); return -1; }
            n=(size_t)width*(size_t)height;
            if (n > 0x7fffffffUL || size != (uint32_t)(n*2U) || at_terrain_init(t,width,height)) {
                fclose(f); return -1;
            }
            for (i=0;i<n;++i) if (get_u16(f,&t->samples[i])) {
                at_terrain_free(t); fclose(f); return -1;
            }
            if (have_geo) t->geo=geo;
            have_hmap=1;
        }
        if (fseek(f,pos+(long)size+(long)(size&1U),SEEK_SET)!=0) {
            if (have_hmap && t->samples) at_terrain_free(t);
            fclose(f); return -1;
        }
    }
    fclose(f);
    if (!have_head || !have_size || !have_hmap) {
        if (have_hmap && t->samples) at_terrain_free(t);
        return -1;
    }
    return 0;
}
