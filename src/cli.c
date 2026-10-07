#include "amiterrain.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
    puts("AmiTerrain M0");
    puts("usage:");
    puts("  amiterrain generate WIDTH HEIGHT SEED OUTPUT.pgm|OUTPUT.atf");
    puts("  amiterrain info INPUT");
    puts("  amiterrain validate INPUT");
    puts("  amiterrain convert INPUT OUTPUT");
    puts("  amiterrain convert --input-format FORMAT [--width W --height H] INPUT");
    puts("                     --output-format FORMAT OUTPUT");
    puts("formats: pgm, atf, raw16be, vista-array, vistapro-dem, wcs-elev, dted, usgs-dem, hgt, asc, xyz, ilbm");
    puts("note: raw16be and vista-array input require --width and --height");
    puts("      .dem is never auto-detected because legacy DEM formats are ambiguous");
}

static int suffix(const char *s, const char *ext)
{
    size_t a=strlen(s), b=strlen(ext);
    return a>=b && !strcmp(s+a-b,ext);
}

static const char *infer_format(const char *path)
{
    if (suffix(path,".atf")) return "atf";
    if (suffix(path,".pgm")) return "pgm";
    if (suffix(path,".raw")) return "raw16be";
    if (suffix(path,".elev")) return "wcs-elev";
    if (suffix(path,".hgt") || suffix(path,".HGT")) return "hgt";
    if (suffix(path,".asc") || suffix(path,".ASC")) return "asc";
    if (suffix(path,".xyz") || suffix(path,".XYZ")) return "xyz";
    if (suffix(path,".ilbm") || suffix(path,".ILBM")) return "ilbm";
    return 0;
}

static int load_as(const char *fmt, const char *path, uint32_t w, uint32_t h, ATTerrain *t)
{
    if (!fmt || !strcmp(fmt,"auto")) fmt=infer_format(path);
    if (!fmt) return -1;
    if (!strcmp(fmt,"atf")) return at_read_atf(path,t);
    if (!strcmp(fmt,"pgm")) return at_read_pgm16(path,t);
    if (!strcmp(fmt,"wcs-elev")) return at_read_wcs_elev(path,t);
    if (!strcmp(fmt,"vistapro-dem")) return at_read_vistapro_dem(path,t);
    if (!strcmp(fmt,"dted")) return at_read_dted(path,t);
    if (!strcmp(fmt,"usgs-dem")) return at_read_usgs_dem(path,t);
    if (!strcmp(fmt,"hgt")) return at_read_srtm_hgt(path,t);
    if (!strcmp(fmt,"asc")) return at_read_esri_ascii_grid(path,t);
    if (!strcmp(fmt,"xyz")) return at_read_xyz_grid(path,t);
    if (!strcmp(fmt,"ilbm")) return at_read_ilbm_heightmap(path,t);
    if (!strcmp(fmt,"raw16be")) return w && h ? at_read_raw16be(path,w,h,t) : -1;
    if (!strcmp(fmt,"vista-array")) return w && h ? at_read_vistapro_binary(path,w,h,t) : -1;
    return -1;
}

static int save_as(const char *fmt, const char *path, const ATTerrain *t)
{
    if (!fmt || !strcmp(fmt,"auto")) fmt=infer_format(path);
    if (!fmt) return -1;
    if (!strcmp(fmt,"atf")) return at_write_atf(path,t);
    if (!strcmp(fmt,"pgm")) return at_write_pgm16(path,t);
    if (!strcmp(fmt,"raw16be")) return at_write_raw16be(path,t);
    if (!strcmp(fmt,"vista-array")) return at_write_vistapro_binary(path,t);
    if (!strcmp(fmt,"wcs-elev")) return at_write_wcs_elev(path,t);
    return -1;
}

static void print_info(const ATTerrain *t)
{
    printf("%lux%lu checksum=%08lx",(unsigned long)t->width,(unsigned long)t->height,
           (unsigned long)at_checksum(t));
    if (t->geo.valid)
        printf(" geo=(%.10g,%.10g step %.10g,%.10g elev-scale %.10g)",
               t->geo.origin_lat,t->geo.origin_lon,t->geo.step_lat,t->geo.step_lon,
               t->geo.elevation_scale);
    putchar('\n');
}

int main(int argc, char **argv)
{
    ATTerrain t={0};
    if (argc<2) { usage(); return 2; }

    if (!strcmp(argv[1],"generate") && argc==6) {
        uint32_t w=(uint32_t)strtoul(argv[2],0,0), h=(uint32_t)strtoul(argv[3],0,0);
        uint32_t seed=(uint32_t)strtoul(argv[4],0,0);
        if (at_terrain_init(&t,w,h)) return 1;
        at_generate_noise(&t,seed);
        if (save_as("auto",argv[5],&t)) { at_terrain_free(&t); return 1; }
        printf("%lux%lu seed=%lu checksum=%08lx\n",(unsigned long)w,(unsigned long)h,
               (unsigned long)seed,(unsigned long)at_checksum(&t));
        at_terrain_free(&t); return 0;
    }

    if ((!strcmp(argv[1],"info") || !strcmp(argv[1],"validate")) && argc==3) {
        if (load_as("auto",argv[2],0,0,&t)) return 1;
        print_info(&t); at_terrain_free(&t); return 0;
    }

    if (!strcmp(argv[1],"convert") && argc==4) {
        if (load_as("auto",argv[2],0,0,&t)) return 1;
        if (save_as("auto",argv[3],&t)) { at_terrain_free(&t); return 1; }
        at_terrain_free(&t); return 0;
    }

    if (!strcmp(argv[1],"convert")) {
        const char *infmt=0,*outfmt=0,*in=0,*out=0; uint32_t w=0,h=0; int i;
        for(i=2;i<argc;++i) {
            if (!strcmp(argv[i],"--input-format") && i+1<argc) infmt=argv[++i];
            else if (!strcmp(argv[i],"--output-format") && i+1<argc) outfmt=argv[++i];
            else if (!strcmp(argv[i],"--width") && i+1<argc) w=(uint32_t)strtoul(argv[++i],0,0);
            else if (!strcmp(argv[i],"--height") && i+1<argc) h=(uint32_t)strtoul(argv[++i],0,0);
            else if (!in) in=argv[i]; else if (!out) out=argv[i]; else { usage(); return 2; }
        }
        if (!in || !out || !infmt || !outfmt) { usage(); return 2; }
        if (load_as(infmt,in,w,h,&t)) { fprintf(stderr,"cannot read %s as %s\n",in,infmt); return 1; }
        if (save_as(outfmt,out,&t)) { fprintf(stderr,"cannot write %s as %s\n",out,outfmt); at_terrain_free(&t); return 1; }
        at_terrain_free(&t); return 0;
    }

    usage(); return 2;
}
