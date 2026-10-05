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

static int at_dted_coord(const unsigned char *p, int lon, double *out)
{
    unsigned int deg=0,min=0,sec=0; size_t nd=lon?3U:2U,i;
    for(i=0;i<nd;++i) { if(p[i]<'0'||p[i]>'9') return -1; deg=deg*10U+(unsigned)(p[i]-'0'); }
    for(i=nd;i<nd+2U;++i) { if(p[i]<'0'||p[i]>'9') return -1; min=min*10U+(unsigned)(p[i]-'0'); }
    for(i=nd+2U;i<nd+4U;++i) { if(p[i]<'0'||p[i]>'9') return -1; sec=sec*10U+(unsigned)(p[i]-'0'); }
    if((lon && p[7]!='E' && p[7]!='W') || (!lon && p[6]!='N' && p[6]!='S') || min>59U || sec>59U) return -1;
    *out=(double)deg+(double)min/60.0+(double)sec/3600.0;
    if((lon?p[7]:p[6])=='W' || (lon?p[7]:p[6])=='S') *out=-*out;
    return 0;
}

static int at_dted_interval(const unsigned char *p, double *out)
{
    uint32_t v; if(at_dted_ascii_u32(p,4,&v)) return -1;
    *out=(double)v/36000.0; return 0; /* tenths of arc-second -> degrees */
}

int at_read_dted(const char *path, ATTerrain *t)
{
    FILE *f; unsigned char uhl[80], head[8], eb[2], sum[4]; uint32_t w,h,col,y;
    if(!path||!t) return -1; f=fopen(path,"rb"); if(!f) return -1;
    if(fread(uhl,1,80,f)!=80 || memcmp(uhl,"UHL1",4) ||
       at_dted_ascii_u32(uhl+47,4,&w) || at_dted_ascii_u32(uhl+51,4,&h) ||
       !w || !h) { fclose(f); return -1; }
    if(fseek(f,648+2700,SEEK_CUR) || at_terrain_init(t,w,h)) { fclose(f); return -1; }
    {
        double swlon,swlat,dlon,dlat;
        if(!at_dted_coord(uhl+4,1,&swlon) && !at_dted_coord(uhl+12,0,&swlat) &&
           !at_dted_interval(uhl+20,&dlon) && !at_dted_interval(uhl+24,&dlat)) {
            t->geo.valid=1;
            t->geo.origin_lon=swlon;
            t->geo.origin_lat=swlat+(double)(h-1U)*dlat; /* canonical top/north row */
            t->geo.step_lon=dlon;
            t->geo.step_lat=-dlat;
            t->geo.elevation_scale=1.0;
            t->geo.crs_type=AT_CRS_GEOGRAPHIC;
            t->geo.coordinate_units=AT_COORD_UNITS_DEGREES;
            t->geo.transform[0]=t->geo.origin_lon; t->geo.transform[1]=t->geo.step_lon; t->geo.transform[2]=0.0;
            t->geo.transform[3]=t->geo.origin_lat; t->geo.transform[4]=0.0; t->geo.transform[5]=t->geo.step_lat;
        }
    }
    for(col=0;col<w;++col) {
        uint32_t lon, checksum=0, stored; size_t k;
        if(fread(head,1,8,f)!=8 || head[0]!=0xaa) goto fail;
        lon=((uint32_t)head[4]<<8)|head[5];
        if(lon!=col) goto fail;
        for(k=0;k<8;++k) checksum+=head[k];
        for(y=0;y<h;++y) {
            uint16_t raw,mag; int32_t elev;
            if(fread(eb,1,2,f)!=2) goto fail;
            checksum+=(uint32_t)eb[0]+(uint32_t)eb[1];
            raw=(uint16_t)(((uint16_t)eb[0]<<8)|eb[1]); mag=(uint16_t)(raw&0x7fffU);
            elev=(raw&0x8000U)?-(int32_t)mag:(int32_t)mag;
            /* DTED void is signed-magnitude -32767. Canonical uint16 value 0
               is reserved here for missing elevation; -32768 is not representable
               by DTED signed magnitude and therefore remains unambiguous. */
            t->samples[(size_t)(h-1U-y)*w+col]=(raw==0xffffU)?0U:(uint16_t)(elev+32768);
        }
        if(fread(sum,1,4,f)!=4) goto fail;
        stored=((uint32_t)sum[0]<<24)|((uint32_t)sum[1]<<16)|((uint32_t)sum[2]<<8)|sum[3];
        if(checksum!=stored) goto fail;
    }
    /* ground_units==3 denotes arc-seconds in classic USGS DEM. Profile X/Y
       are then geographic coordinates in arc-seconds; convert to degrees. */
    if(ground_units==3 && dx>0.0 && dy>0.0) {
        t->geo.valid=1;
        t->geo.origin_lon=first_x/3600.0;
        t->geo.origin_lat=(first_y+(double)(t->height-1U)*dy)/3600.0;
        t->geo.step_lon=dx/3600.0;
        t->geo.step_lat=-dy/3600.0;
        t->geo.elevation_scale=1.0;
        t->geo.crs_type=AT_CRS_GEOGRAPHIC;
        t->geo.coordinate_units=AT_COORD_UNITS_DEGREES;
        t->geo.transform[0]=t->geo.origin_lon; t->geo.transform[1]=t->geo.step_lon; t->geo.transform[2]=0.0;
        t->geo.transform[3]=t->geo.origin_lat; t->geo.transform[4]=0.0; t->geo.transform[5]=t->geo.step_lat;
    } else if((ground_units==1 || ground_units==2) && dx>0.0 && dy>0.0) {
        /* Projected USGS DEM: preserve the native grid without guessing a CRS
           authority code. ground_units 1=feet, 2=metres. */
        t->geo.valid=1;
        t->geo.crs_type=AT_CRS_PROJECTED;
        t->geo.coordinate_units=(ground_units==1)?AT_COORD_UNITS_FEET:AT_COORD_UNITS_METERS;
        t->geo.epsg=at_usgs_epsg(proj_sys,proj_zone,hdatum);
        t->geo.projection_system=(int32_t)proj_sys;
        t->geo.projection_zone=(int32_t)proj_zone;
        t->geo.horizontal_datum=(int32_t)hdatum;
        t->geo.elevation_scale=1.0;
        t->geo.transform[0]=first_x; t->geo.transform[1]=dx; t->geo.transform[2]=0.0;
        t->geo.transform[3]=first_y+(double)(t->height-1U)*dy;
        t->geo.transform[4]=0.0; t->geo.transform[5]=-dy;
    }
    fclose(f); return 0;
fail:
    at_terrain_free(t); fclose(f); return -1;
}


/* Classic USGS DEM ASCII reader, initial rectangular-profile subset.
   The format is one 1024-byte A record followed by B profile records.
   B profiles are west-to-east; elevations within each profile are south-to-north. */
static int at_usgs_i6(const char *p, long *v)
{
    char b[7]; char *e; memcpy(b,p,6); b[6]=0; *v=strtol(b,&e,10);
    return e==b ? -1 : 0;
}
static int at_usgs_d24(const char *p, double *v)
{
    char b[25],*q,*e; memcpy(b,p,24); b[24]=0;
    for(q=b;*q;++q) if(*q=='D'||*q=='d') *q='E';
    *v=strtod(b,&e); return e==b ? -1 : 0;
}
static int32_t at_usgs_epsg(long proj_sys, long zone, long datum)
{
    /* USGS DEM projection code 1 is UTM. Datum codes used here follow the
       classic DEM horizontal datum field: 1=NAD27, 2=WGS72, 3=WGS84, 4=NAD83. */
    if(proj_sys!=1 || zone<1 || zone>60) return 0;
    if(datum==1) return (int32_t)(26700+zone); /* NAD27 / UTM north */
    if(datum==4) return (int32_t)(26900+zone); /* NAD83 / UTM north */
    if(datum==3) return (int32_t)(32600+zone); /* WGS84 / UTM north */
    return 0;
}

int at_read_usgs_dem(const char *path, ATTerrain *t)
{
    FILE *f; char a[1024],bh[144],field[7]; long rows,cols,pr,pc,n,one,ground_units,proj_sys=0,proj_zone=0,hdatum=0; uint32_t x,y; double x0,y0,z0,dx,dy,first_x=0.0,first_y=0.0;
    if(!path||!t) return -1; f=fopen(path,"rb"); if(!f) return -1;
    if(fread(a,1,1024,f)!=1024) { fclose(f); return -1; }
    /* A-record ground reference system is at 156; units/resolution are later.
       Only geographic arc-second DEMs are mapped to ATGeoMetadata lat/lon. */
    if(at_usgs_i6(a+156,&proj_sys)) proj_sys=0;
    if(at_usgs_i6(a+162,&proj_zone)) proj_zone=0;
    if(at_usgs_i6(a+528,&ground_units)) ground_units=0;
    /* Horizontal datum code is an optional later A-record field. */
    if(at_usgs_i6(a+890,&hdatum)) hdatum=0;
    if(at_usgs_d24(a+816,&dx)) dx=0.0;
    if(at_usgs_d24(a+840,&dy)) dy=0.0;
    /* A-record elements 21/22: rows of profiles (normally 1), columns of profiles. */
    if(at_usgs_i6(a+852,&rows)||at_usgs_i6(a+858,&cols)||rows!=1||cols<=0) { fclose(f); return -1; }
    /* Read first B header to discover the rectangular profile height. */
    if(fread(bh,1,144,f)!=144 || at_usgs_i6(bh,&pr)||at_usgs_i6(bh+6,&pc) ||
       at_usgs_i6(bh+12,&n)||at_usgs_i6(bh+18,&one)||pr!=1||pc!=1||n<=0||one!=1) { fclose(f); return -1; }
    if(at_terrain_init(t,(uint32_t)cols,(uint32_t)n)) { fclose(f); return -1; }
    for(x=0;x<(uint32_t)cols;++x) {
        if(x) {
            if(fread(bh,1,144,f)!=144 || at_usgs_i6(bh,&pr)||at_usgs_i6(bh+6,&pc) ||
               at_usgs_i6(bh+12,&n)||at_usgs_i6(bh+18,&one)||pr!=1||pc!=(long)x+1 ||
               n!=(long)t->height||one!=1) goto fail;
        }
        if(at_usgs_d24(bh+24,&x0)||at_usgs_d24(bh+48,&y0)||at_usgs_d24(bh+72,&z0)) goto fail;
        if(x==0) { first_x=x0; first_y=y0; }
        for(y=0;y<t->height;++y) {
            long ev; int32_t elev;
            if(fread(field,1,6,f)!=6) goto fail; field[6]=0;
            { char *e; ev=strtol(field,&e,10); if(e==field) goto fail; }
            elev=(int32_t)ev+(int32_t)z0;
            if(elev < -32768 || elev > 32767) goto fail;
            t->samples[(size_t)(t->height-1U-y)*t->width+x]=(uint16_t)(elev+32768);
        }
        /* B profiles are padded to a 1024-byte logical-record boundary and may
           span any number of records; elevation fields remain contiguous. */
        { long used=144L+6L*(long)t->height; long pad=(1024L-(used%1024L))%1024L;
          if(pad && fseek(f,pad,SEEK_CUR)) goto fail; }
    }
    fclose(f); return 0;
fail:
    at_terrain_free(t); fclose(f); return -1;
}
