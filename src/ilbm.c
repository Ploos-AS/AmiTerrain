#include "amiterrain.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t be16(const unsigned char *p){ return (uint16_t)(((uint16_t)p[0]<<8)|p[1]); }
static uint32_t be32(const unsigned char *p){ return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }

static int byterun1(FILE *f,unsigned char *dst,size_t n){
    size_t o=0;
    while(o<n){
        int q=fgetc(f); signed char c; size_t k; int v;
        if(q==EOF) return -1; c=(signed char)q;
        if(c>=0){ k=(size_t)c+1U; if(k>n-o||fread(dst+o,1,k,f)!=k) return -1; o+=k; }
        else if(c!=-128){ k=(size_t)(1-(int)c); v=fgetc(f); if(v==EOF||k>n-o) return -1; memset(dst+o,v,k); o+=k; }
    }
    return 0;
}

int at_read_ilbm_heightmap(const char *path, ATTerrain *t){
    FILE *f; unsigned char h[12],ch[8],bmhd[20]; uint32_t form_left,sz; long body=-1; uint16_t w=0,hgt=0; unsigned char planes=0,mask=0,comp=0; size_t rb,plane_bytes; unsigned char *row=0; uint32_t y,x,p;
    if(!path||!t) return -1; f=fopen(path,"rb"); if(!f) return -1;
    if(fread(h,1,12,f)!=12||memcmp(h,"FORM",4)||memcmp(h+8,"ILBM",4)){ fclose(f); return -1; }
    form_left=be32(h+4); if(form_left<4){ fclose(f); return -1; } form_left-=4;
    while(form_left>=8 && fread(ch,1,8,f)==8){
        sz=be32(ch+4); form_left-=8;
        if(sz>form_left){ fclose(f); return -1; }
        if(!memcmp(ch,"BMHD",4)){
            if(sz<20||fread(bmhd,1,20,f)!=20){ fclose(f); return -1; }
            w=be16(bmhd); hgt=be16(bmhd+2); planes=bmhd[8]; mask=bmhd[9]; comp=bmhd[10];
            if(sz>20 && fseek(f,(long)(sz-20),SEEK_CUR)){ fclose(f); return -1; }
        } else if(!memcmp(ch,"BODY",4)){ body=ftell(f); if(fseek(f,(long)sz,SEEK_CUR)){ fclose(f); return -1; } }
        else if(fseek(f,(long)sz,SEEK_CUR)){ fclose(f); return -1; }
        if(sz&1U){ if(fgetc(f)==EOF){ fclose(f); return -1; } }
        form_left-=sz+(sz&1U);
    }
    if(!w||!hgt||planes<1||planes>8||mask>1||comp>1||body<0){ fclose(f); return -1; }
    rb=((size_t)w+15U)/16U*2U; plane_bytes=rb*((size_t)planes+(mask==1?1U:0U));
    row=(unsigned char*)malloc(plane_bytes); if(!row){ fclose(f); return -1; }
    if(fseek(f,body,SEEK_SET)||at_terrain_init(t,w,hgt)){ free(row); fclose(f); return -1; }
    for(y=0;y<hgt;++y){
        if(comp==0){ if(fread(row,1,plane_bytes,f)!=plane_bytes) goto fail; }
        else { for(p=0;p<(uint32_t)planes+(mask==1?1U:0U);++p) if(byterun1(f,row+p*rb,rb)) goto fail; }
        for(x=0;x<w;++x){
            unsigned v=0; for(p=0;p<planes;++p) if(row[p*rb+x/8U]&(0x80U>>(x&7U))) v|=1U<<p;
            t->samples[(size_t)y*w+x]=(uint16_t)((v*65535U)/((1U<<planes)-1U));
        }
    }
    free(row); fclose(f); return 0;
fail:
    at_terrain_free(t); free(row); fclose(f); return -1;
}
