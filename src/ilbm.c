#include "amiterrain.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t be16(const unsigned char *p){ return (uint16_t)(((uint16_t)p[0]<<8)|p[1]); }
static uint32_t be32(const unsigned char *p){ return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }

static int byterun1(FILE *f,unsigned char *dst,size_t n,size_t *remaining){
    size_t o=0;
    while(o<n){
        int q; signed char c; size_t k; int v;
        if(!*remaining) return -1; q=fgetc(f); if(q==EOF) return -1; --*remaining; c=(signed char)q;
        if(c>=0){ k=(size_t)c+1U; if(k>n-o||k>*remaining||fread(dst+o,1,k,f)!=k) return -1; *remaining-=k; o+=k; }
        else if(c!=-128){ k=(size_t)(1-(int)c); if(!*remaining||k>n-o) return -1; v=fgetc(f); if(v==EOF) return -1; --*remaining; memset(dst+o,v,k); o+=k; }
    }
    return 0;
}

int at_read_ilbm_heightmap(const char *path, ATTerrain *t){
    FILE *f; unsigned char h[12],ch[8],bmhd[20]; uint32_t form_left,sz; long body=-1; uint32_t body_size=0; uint16_t w=0,hgt=0; unsigned char planes=0,mask=0,comp=0; uint32_t camg=0; int have_camg=0; size_t rb,plane_bytes,body_remaining; unsigned char *row=0; uint32_t y,x,p;
    if(!path||!t) return -1; f=fopen(path,"rb"); if(!f) return -1;
    if(fread(h,1,12,f)!=12||memcmp(h,"FORM",4)||memcmp(h+8,"ILBM",4)){ fclose(f); return -1; }
    form_left=be32(h+4); if(form_left<4){ fclose(f); return -1; } form_left-=4;
    while(form_left>=8 && fread(ch,1,8,f)==8){
        sz=be32(ch+4); form_left-=8;
        if(sz>form_left || (sz&1U)>(form_left-sz)){ fclose(f); return -1; }
        if(!memcmp(ch,"BMHD",4)){
            if(sz<20||fread(bmhd,1,20,f)!=20){ fclose(f); return -1; }
            w=be16(bmhd); hgt=be16(bmhd+2); planes=bmhd[8]; mask=bmhd[9]; comp=bmhd[10];
            if(sz>20 && fseek(f,(long)(sz-20),SEEK_CUR)){ fclose(f); return -1; }
        } else if(!memcmp(ch,"CAMG",4)){
            unsigned char m[4]; if(sz<4||fread(m,1,4,f)!=4){ fclose(f); return -1; }
            camg=be32(m); have_camg=1; if(sz>4 && fseek(f,(long)(sz-4),SEEK_CUR)){ fclose(f); return -1; }
        } else if(!memcmp(ch,"BODY",4)){ if(body>=0){ fclose(f); return -1; } body=ftell(f); body_size=sz; if(fseek(f,(long)sz,SEEK_CUR)){ fclose(f); return -1; } }
        else if(fseek(f,(long)sz,SEEK_CUR)){ fclose(f); return -1; }
        if(sz&1U){ if(fgetc(f)==EOF){ fclose(f); return -1; } }
        form_left-=sz+(sz&1U);
    }
    if(form_left!=0 || !w||!hgt||planes<1||planes>8||mask>1||comp>1||body<0){ fclose(f); return -1; }
    /* HAM (0x0800) and EHB (0x0080) encode display colour semantics, not a linear height index. */
    if(have_camg && (camg & (0x0800U|0x0080U))){ fclose(f); return -1; }
    rb=((size_t)w+15U)/16U*2U; plane_bytes=rb*((size_t)planes+(mask==1?1U:0U));
    row=(unsigned char*)malloc(plane_bytes); if(!row){ fclose(f); return -1; }
    if(fseek(f,body,SEEK_SET)||at_terrain_init(t,w,hgt)){ free(row); fclose(f); return -1; }
    body_remaining=body_size;
    for(y=0;y<hgt;++y){
        if(comp==0){ if(plane_bytes>body_remaining||fread(row,1,plane_bytes,f)!=plane_bytes) goto fail; body_remaining-=plane_bytes; }
        else { for(p=0;p<(uint32_t)planes+(mask==1?1U:0U);++p) if(byterun1(f,row+p*rb,rb,&body_remaining)) goto fail; }
        for(x=0;x<w;++x){
            unsigned v=0; for(p=0;p<planes;++p) if(row[p*rb+x/8U]&(0x80U>>(x&7U))) v|=1U<<p;
            t->samples[(size_t)y*w+x]=(uint16_t)((v*65535U)/((1U<<planes)-1U));
        }
    }
    if(ftell(f)<body || (unsigned long)(ftell(f)-body)>(unsigned long)body_size) goto fail;
    free(row); fclose(f); return 0;
fail:
    at_terrain_free(t); free(row); fclose(f); return -1;
}


static int put_be16(FILE *f,uint16_t v){ return fputc((v>>8)&255,f)==EOF||fputc(v&255,f)==EOF?-1:0; }
static int put_be32(FILE *f,uint32_t v){ return fputc((v>>24)&255,f)==EOF||fputc((v>>16)&255,f)==EOF||fputc((v>>8)&255,f)==EOF||fputc(v&255,f)==EOF?-1:0; }

int at_write_ilbm_heightmap(const char *path,const ATTerrain *t){
    FILE *f; size_t rb,body; uint32_t form; uint32_t y,x,p; unsigned char *row;
    if(!path||!t||!t->samples||!t->width||!t->height||t->width>65535U||t->height>65535U) return -1;
    rb=((size_t)t->width+15U)/16U*2U; body=rb*8U*(size_t)t->height;
    if(body>0xffffffffU-816U) return -1; form=(uint32_t)(4U+8U+20U+8U+768U+8U+body);
    f=fopen(path,"wb"); if(!f) return -1;
    if(fwrite("FORM",1,4,f)!=4||put_be32(f,form)||fwrite("ILBM",1,4,f)!=4||
       fwrite("BMHD",1,4,f)!=4||put_be32(f,20)||put_be16(f,(uint16_t)t->width)||put_be16(f,(uint16_t)t->height)||
       put_be16(f,0)||put_be16(f,0)||fputc(8,f)==EOF||fputc(0,f)==EOF||fputc(0,f)==EOF||fputc(0,f)==EOF||
       put_be16(f,0)||fputc(10,f)==EOF||fputc(10,f)==EOF||put_be16(f,(uint16_t)t->width)||put_be16(f,(uint16_t)t->height)){ fclose(f); return -1; }
    if(fwrite("CMAP",1,4,f)!=4||put_be32(f,768U)){ fclose(f); return -1; }
    { unsigned i; for(i=0;i<256U;++i) if(fputc((int)i,f)==EOF||fputc((int)i,f)==EOF||fputc((int)i,f)==EOF){ fclose(f); return -1; } }
    if(fwrite("BODY",1,4,f)!=4||put_be32(f,(uint32_t)body)){ fclose(f); return -1; }
    row=(unsigned char*)malloc(rb); if(!row){ fclose(f); return -1; }
    for(y=0;y<t->height;++y) for(p=0;p<8;++p){
        memset(row,0,rb);
        for(x=0;x<t->width;++x){
            uint16_t s=t->samples[(size_t)y*t->width+x]; unsigned v=(unsigned)((s+128U)/257U);
            if(v>255U) v=255U; if(v&(1U<<p)) row[x/8U]|=(unsigned char)(0x80U>>(x&7U));
        }
        if(fwrite(row,1,rb,f)!=rb){ free(row); fclose(f); return -1; }
    }
    free(row); return fclose(f)==0?0:-1;
}
