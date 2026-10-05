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


int at_read_vistapro_dem(const char *path, ATTerrain *t)
{
    FILE *f; unsigned char hdr[144], *packed=0, *tmp=0; uint32_t compression,w,h,row; size_t cap;
    if(!path || !t) return -1;
    f=fopen(path,"rb"); if(!f) return -1;
    if(fread(hdr,1,sizeof(hdr),f)!=sizeof(hdr) || memcmp(hdr,"Vista DEM File",14)) { fclose(f); return -1; }
    compression=at_be32(hdr+128); w=at_be32(hdr+136); h=at_be32(hdr+140);
    if(!compression || w!=h || (w!=258 && w!=514 && w!=1026 && w!=2050)) { fclose(f); return -1; }
    if(at_terrain_init(t,w,h)) { fclose(f); return -1; }
    cap=(size_t)w*3U+16U;
    packed=(unsigned char*)malloc(cap); tmp=(unsigned char*)malloc(cap);
    if(!packed || !tmp || fseek(f,2048,SEEK_SET)) goto fail;
    for(row=0;row<h;++row) {
        int a=fgetc(f),b=fgetc(f); size_t count,pi=0,ti=0,x;
        if(a==EOF||b==EOF) goto fail; count=((size_t)a<<8)|(unsigned)b;
        if(!count || count>cap || fread(packed,1,count,f)!=count) goto fail;
        while(pi<count) {
            int8_t code=(int8_t)packed[pi++]; size_t n,j;
            if(code<=0) {
                if(pi>=count) goto fail; n=(size_t)(1-(int)code);
                if(ti+n>cap) goto fail; for(j=0;j<n;++j) tmp[ti++]=packed[pi]; ++pi;
            } else {
                n=(size_t)code+1U; if(pi+n>count || ti+n>cap) goto fail;
                memcpy(tmp+ti,packed+pi,n); ti+=n; pi+=n;
            }
        }
        if(ti<2) goto fail;
        {
            size_t p=2; int32_t elev=(int16_t)(((uint16_t)tmp[0]<<8)|tmp[1]);
            uint16_t *dst=t->samples+(size_t)(h-1U-row)*w;
            dst[0]=(uint16_t)(elev+32768);
            for(x=1;x<w;++x) {
                int8_t d; if(p>=ti) goto fail; d=(int8_t)tmp[p++];
                if(d==-128) { if(p+1>=ti) goto fail; elev=(int16_t)(((uint16_t)tmp[p]<<8)|tmp[p+1]); p+=2; }
                else elev+=d;
                if(elev<-32768 || elev>32767) goto fail;
                dst[x]=(uint16_t)(elev+32768);
            }
        }
    }
    free(tmp); free(packed); fclose(f); return 0;
fail:
    free(tmp); free(packed); at_terrain_free(t); fclose(f); return -1;
}


static int at_dted_ascii_u32(const unsigned char *p, size_t n, uint32_t *out)
{
    uint32_t v=0; size_t i; if(!p||!out||!n) return -1;
    for(i=0;i<n;++i) { if(p[i]<'0'||p[i]>'9') return -1; v=v*10U+(uint32_t)(p[i]-'0'); }
    *out=v; return 0;
}

int at_read_dted(const char *path, ATTerrain *t)
{
    FILE *f; unsigned char uhl[80], head[8], eb[2], sum[4]; uint32_t w,h,col,y;
    if(!path||!t) return -1; f=fopen(path,"rb"); if(!f) return -1;
    if(fread(uhl,1,80,f)!=80 || memcmp(uhl,"UHL1",4) ||
       at_dted_ascii_u32(uhl+47,4,&w) || at_dted_ascii_u32(uhl+51,4,&h) ||
       !w || !h) { fclose(f); return -1; }
    if(fseek(f,648+2700,SEEK_CUR) || at_terrain_init(t,w,h)) { fclose(f); return -1; }
    for(col=0;col<w;++col) {
        uint32_t lon;
        if(fread(head,1,8,f)!=8 || head[0]!=0xaa) goto fail;
        lon=((uint32_t)head[4]<<8)|head[5];
        if(lon!=col) goto fail;
        for(y=0;y<h;++y) {
            uint16_t raw,mag; int32_t elev;
            if(fread(eb,1,2,f)!=2) goto fail;
            raw=(uint16_t)(((uint16_t)eb[0]<<8)|eb[1]); mag=(uint16_t)(raw&0x7fffU);
            elev=(raw&0x8000U)?-(int32_t)mag:(int32_t)mag;
            t->samples[(size_t)(h-1U-y)*w+col]=(uint16_t)(elev+32768);
        }
        if(fread(sum,1,4,f)!=4) goto fail;
    }
    fclose(f); return 0;
fail:
    at_terrain_free(t); fclose(f); return -1;
}
