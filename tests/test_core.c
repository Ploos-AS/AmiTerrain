#include "amiterrain.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    ATTerrain a={0}, b={0};
    uint32_t ca, cb;
    assert(at_terrain_init(&a,32,32)==0);
    assert(at_terrain_init(&b,32,32)==0);
    at_generate_noise(&a,6502);
    at_generate_noise(&b,6502);
    ca=at_checksum(&a); cb=at_checksum(&b);
    assert(ca==cb);

    assert(at_write_pgm16("test-roundtrip.pgm",&a)==0);
    at_terrain_free(&b);
    assert(at_read_pgm16("test-roundtrip.pgm",&b)==0);
    assert(at_checksum(&b)==ca);

    assert(at_write_atf("test-roundtrip.atf",&a)==0);
    at_terrain_free(&b);
    assert(at_read_atf("test-roundtrip.atf",&b)==0);
    assert(b.width==a.width && b.height==a.height);
    assert(at_checksum(&b)==ca);

    assert(at_write_vistapro_binary("test-vistapro.dem",&a)==0);
    at_terrain_free(&b);
    assert(at_read_vistapro_binary("test-vistapro.dem",a.width,a.height,&b)==0);
    assert(b.width==a.width && b.height==a.height);
    assert(at_checksum(&b)==ca);

    /* Synthetic native Amiga WCS ELEV v1.02 fixture:
       big-endian float version, 64-byte header, then signed 16-bit elevations. */
    {
        FILE *wf=fopen("test-wcs.elev","wb");
        unsigned char wh[64]={0};
        const unsigned char ver[4]={0x3f,0x82,0x8f,0x5c}; /* IEEE-754 1.02f */
        const int16_t vals[6]={-32768,-1,0,1,1234,32767};
        size_t i;
        assert(wf!=NULL);
        wh[3]=1; /* rows=1 => height=2 */
        wh[7]=3; /* columns=3 */
        assert(fwrite(ver,1,4,wf)==4);
        assert(fwrite(wh,1,64,wf)==64);
        for(i=0;i<6;++i) {
            uint16_t u=(uint16_t)vals[i];
            assert(fputc((int)(u>>8),wf)!=EOF);
            assert(fputc((int)(u&255),wf)!=EOF);
        }
        assert(fclose(wf)==0);
        at_terrain_free(&b);
        assert(at_read_wcs_elev("test-wcs.elev",&b)==0);
        assert(b.width==3 && b.height==2);
        for(i=0;i<6;++i)
            assert(b.samples[i]==(uint16_t)((int32_t)vals[i]+32768));
    }


    /* Synthetic native VistaPro DEM: 258 rows, literal ByteRun1 stream.
       First source row is south and must become AmiTerrain's last row. */
    {
        FILE *vf=fopen("test-vistapro-native.dem","wb");
        unsigned char hdr[2048]={0}; size_t row,x;
        assert(vf!=NULL);
        memcpy(hdr,"Vista DEM File",14);
        hdr[131]=1; /* BE32 compression value 1 at offset 128 */
        hdr[138]=1; hdr[139]=2; /* width 258: BE32 at offset 136 */
        hdr[142]=1; hdr[143]=2; /* height 258: BE32 at offset 140 */
        assert(fwrite(hdr,1,sizeof(hdr),vf)==sizeof(hdr));
        for(row=0;row<258;++row) {
            unsigned char raw[261], packed[264]; size_t p=0;
            int16_t base=(int16_t)(100+(int)row);
            raw[0]=(unsigned char)(((uint16_t)base)>>8); raw[1]=(unsigned char)base;
            for(x=2;x<259;++x) raw[x]=1; /* 257 deltas for 258 samples */
            if(row==0) { /* force VistaPro's elevation -128 escape/new-base path */
                raw[2]=0x80; raw[3]=0x01; raw[4]=0xf4; /* new base = 500 */
                for(x=5;x<261;++x) raw[x]=0;
            }
            packed[p++]=127; memcpy(packed+p,raw,128); p+=128;
            packed[p++]=127; memcpy(packed+p,raw+128,128); p+=128;
            packed[p++]=4; memcpy(packed+p,raw+256,5); p+=5;
            assert(fputc((int)(p>>8),vf)!=EOF); assert(fputc((int)(p&255),vf)!=EOF);
            assert(fwrite(packed,1,p,vf)==p);
            assert(p==264);
        }
        assert(fclose(vf)==0);
        at_terrain_free(&b);
        assert(at_read_vistapro_dem("test-vistapro-native.dem",&b)==0);
        assert(b.width==258 && b.height==258);
        assert(b.samples[(257U*258U)]==(uint16_t)(100+32768));
        assert(b.samples[(257U*258U)+1U]==(uint16_t)(500+32768));
        assert(b.samples[0]==(uint16_t)(357+32768));
        assert(b.samples[257]==(uint16_t)(357+257+32768));
    }

    /* Native VistaPro truncation must be rejected without retaining terrain. */
    {
        FILE *src=fopen("test-vistapro-native.dem","rb"), *dst=fopen("test-vistapro-truncated.dem","wb");
        unsigned char buf[2100]; size_t n;
        assert(src && dst); n=fread(buf,1,sizeof(buf),src); assert(n==sizeof(buf));
        assert(fwrite(buf,1,n,dst)==n); fclose(src); fclose(dst);
        at_terrain_free(&b);
        assert(at_read_vistapro_dem("test-vistapro-truncated.dem",&b)!=0);
        assert(b.samples==NULL);
    }

    /* Synthetic DTED: UHL + DSI + ACC, two west-to-east profiles,
       each containing three south-to-north signed-magnitude elevations. */
    {
        FILE *df=fopen("test.dted","wb");
        unsigned char uhl[80]={0}, zeros[3348]={0};
        const int vals[2][3]={{-12,0,34},{100,-200,300}};
        unsigned int col,row;
        assert(df!=NULL);
        memcpy(uhl,"UHL1",4);
        memcpy(uhl+4,"0070000E",8);  /* 7E */
        memcpy(uhl+12,"580000N",7);  /* 58N */
        memcpy(uhl+20,"0360",4);     /* 36.0 arcsec = 0.01 degree */
        memcpy(uhl+24,"0360",4);
        memcpy(uhl+47,"0002",4); memcpy(uhl+51,"0003",4);
        assert(fwrite(uhl,1,80,df)==80);
        assert(fwrite(zeros,1,sizeof(zeros),df)==sizeof(zeros));
        for(col=0;col<2;++col) {
            unsigned char ph[8]={0xaa,0,0,0,0,0,0,0}, tail[4]; unsigned long checksum=0; unsigned int k;
            ph[4]=(unsigned char)(col>>8); ph[5]=(unsigned char)col;
            assert(fwrite(ph,1,8,df)==8); for(k=0;k<8;++k) checksum+=ph[k];
            for(row=0;row<3;++row) {
                int v=vals[col][row]; unsigned int mag,raw,hi,lo;
                if(col==0 && row==1) { raw=0xffffU; } /* DTED void */
                else { mag=(unsigned int)(v<0?-v:v); raw=mag | (v<0?0x8000U:0U); }
                hi=raw>>8; lo=raw&255U; checksum+=hi+lo;
                assert(fputc((int)hi,df)!=EOF); assert(fputc((int)lo,df)!=EOF);
            }
            tail[0]=(unsigned char)(checksum>>24); tail[1]=(unsigned char)(checksum>>16);
            tail[2]=(unsigned char)(checksum>>8); tail[3]=(unsigned char)checksum;
            assert(fwrite(tail,1,4,df)==4);
        }
        assert(fclose(df)==0);
        at_terrain_free(&b);
        assert(at_read_dted("test.dted",&b)==0);
        assert(b.width==2 && b.height==3);
        assert(b.geo.valid);
        assert(b.geo.origin_lon==7.0);
        assert(b.geo.origin_lat>58.019999 && b.geo.origin_lat<58.020001);
        assert(b.geo.step_lon>0.009999 && b.geo.step_lon<0.010001);
        assert(b.geo.step_lat< -0.009999 && b.geo.step_lat> -0.010001);
        assert(b.samples[0]==(uint16_t)(34+32768));
        assert(b.samples[1]==(uint16_t)(300+32768));
        assert(b.samples[2]==0); /* void survives distinctly */
        assert(b.samples[4]==(uint16_t)(-12+32768));
        assert(b.samples[5]==(uint16_t)(100+32768));
    }

    /* Synthetic classic USGS DEM: one A record and two rectangular B profiles. */
    {
        FILE *uf=fopen("test-usgs.dem","wb"); char a[1024],brec[1024]; unsigned int x,y;
        const int vals[2][3]={{10,20,30},{-5,0,15}};
        assert(uf!=NULL); memset(a,' ',sizeof(a));
        { char tmp[64]; snprintf(tmp,sizeof(tmp),"%12.6E",1.0); memcpy(a+840,tmp,12); }
        memcpy(a+852,"     1",6); memcpy(a+858,"     2",6);
        assert(fwrite(a,1,sizeof(a),uf)==sizeof(a));
        for(x=0;x<2;++x) {
            char tmp[64]; memset(brec,' ',sizeof(brec));
            snprintf(tmp,sizeof(tmp),"%6d",1); memcpy(brec,tmp,6);
            snprintf(tmp,sizeof(tmp),"%6u",x+1); memcpy(brec+6,tmp,6);
            memcpy(brec+12,"     3",6); memcpy(brec+18,"     1",6);
            snprintf(tmp,sizeof(tmp),"%24s","0.000000000000000D+00"); memcpy(brec+24,tmp,24);
            snprintf(tmp,sizeof(tmp),"%24s","0.000000000000000D+00"); memcpy(brec+48,tmp,24);
            snprintf(tmp,sizeof(tmp),"%24s","0.000000000000000D+00"); memcpy(brec+72,tmp,24);
            for(y=0;y<3;++y) { snprintf(tmp,sizeof(tmp),"%6d",vals[x][y]); memcpy(brec+144+6*y,tmp,6); }
            assert(fwrite(brec,1,sizeof(brec),uf)==sizeof(brec));
        }
        assert(fclose(uf)==0); at_terrain_free(&b);
        assert(at_read_usgs_dem("test-usgs.dem",&b)==0);
        assert(b.width==2 && b.height==3);
        assert(b.samples[0]==(uint16_t)(30+32768) && b.samples[1]==(uint16_t)(15+32768));
        assert(b.samples[4]==(uint16_t)(10+32768) && b.samples[5]==(uint16_t)(-5+32768));
    }

    /* Projected USGS DEM metadata is preserved without inventing an EPSG code. */
    {
        FILE *uf=fopen("test-usgs-projected.dem","wb"); char a[1024],br[1024],tmp[64]; unsigned int y;
        assert(uf!=NULL); memset(a,' ',sizeof(a));
        memcpy(a+528,"     2",6); /* metres */
        snprintf(tmp,sizeof(tmp),"%12.6E",10.0); memcpy(a+816,tmp,12);
        snprintf(tmp,sizeof(tmp),"%12.6E",20.0); memcpy(a+828,tmp,12);
        snprintf(tmp,sizeof(tmp),"%12.6E",2.0); memcpy(a+840,tmp,12);
        memcpy(a+852,"     1",6); memcpy(a+858,"     1",6);
        assert(fwrite(a,1,sizeof(a),uf)==sizeof(a)); memset(br,' ',sizeof(br));
        memcpy(br,"     1",6); memcpy(br+6,"     1",6); memcpy(br+12,"     3",6); memcpy(br+18,"     1",6);
        snprintf(tmp,sizeof(tmp),"%24s","5.000000000000000D+05"); memcpy(br+24,tmp,24);
        snprintf(tmp,sizeof(tmp),"%24s","6.500000000000000D+06"); memcpy(br+48,tmp,24);
        snprintf(tmp,sizeof(tmp),"%24s","0.000000000000000D+00"); memcpy(br+72,tmp,24);
        for(y=0;y<3;++y) { snprintf(tmp,sizeof(tmp),"%6u",y); memcpy(br+144+6*y,tmp,6); }
        assert(fwrite(br,1,sizeof(br),uf)==sizeof(br)); assert(fclose(uf)==0);
        at_terrain_free(&b); assert(at_read_usgs_dem("test-usgs-projected.dem",&b)==0);
        assert(b.geo.valid && b.geo.crs_type==AT_CRS_PROJECTED);
        assert(b.geo.coordinate_units==AT_COORD_UNITS_METERS && b.geo.epsg==0);
        assert(b.geo.transform[0]==500000.0 && b.geo.transform[1]==10.0);
        assert(b.geo.transform[3]==6500040.0 && b.geo.transform[5]==-20.0);
        assert(b.samples[0]==(uint16_t)(4+32768) && b.samples[2]==32768); /* z resolution 2 */
    }

    /* Regular XYZ grid: preserve X/Y spacing and signed elevations. */
    {
        FILE *xf=fopen("test-grid.xyz","w");
        assert(xf!=NULL);
        fputs("7 59 10\n7.5 59 11\n8 59 12\n7 58.5 -1\n7.5 58.5 0\n8 58.5 1\n",xf);
        assert(fclose(xf)==0);
        at_terrain_free(&b); assert(at_read_xyz_grid("test-grid.xyz",&b)==0);
        assert(b.width==3 && b.height==2);
        assert(b.samples[0]==32778 && b.samples[3]==32767 && b.samples[5]==32769);
        assert(b.geo.valid && b.geo.crs_type==AT_CRS_UNKNOWN);
        assert(b.geo.transform[0]==7.0 && b.geo.transform[1]==0.5);
        assert(b.geo.transform[3]==59.0 && b.geo.transform[5]==-0.5);
    }

    /* Irregular XYZ input is not silently interpolated. */
    {
        FILE *xf=fopen("test-grid-irregular.xyz","w");
        assert(xf!=NULL);
        fputs("0 1 1\n1 1 2\n0 0 3\n1.25 0 4\n",xf);
        assert(fclose(xf)==0);
        at_terrain_free(&b); assert(at_read_xyz_grid("test-grid-irregular.xyz",&b)!=0);
    }

    /* ESRI ASCII Grid: corner origin, nodata and north/top-first affine transform. */
    {
        FILE *af=fopen("test-grid.asc","w");
        assert(af!=NULL);
        fputs("ncols 3\nnrows 2\nxllcorner 100\nyllcorner 200\ncellsize 10\nNODATA_value -9999\n"
              "1 2 -9999\n-1 0 3\n",af);
        assert(fclose(af)==0);
        at_terrain_free(&b); assert(at_read_esri_ascii_grid("test-grid.asc",&b)==0);
        assert(b.width==3 && b.height==2);
        assert(b.samples[0]==32769 && b.samples[1]==32770 && b.samples[2]==0);
        assert(b.samples[3]==32767 && b.samples[4]==32768 && b.samples[5]==32771);
        assert(b.geo.valid && b.geo.crs_type==AT_CRS_UNKNOWN);
        assert(b.geo.transform[0]==105.0 && b.geo.transform[1]==10.0);
        assert(b.geo.transform[3]==215.0 && b.geo.transform[5]==-10.0);
    }

    /* ESRI ASCII Grid center origin must not receive the half-cell offset. */
    {
        FILE *af=fopen("test-grid-center.asc","w");
        assert(af!=NULL);
        fputs("ncols 2\nnrows 2\nxllcenter 7.5\nyllcenter 58.5\ncellsize 0.5\nNODATA_value -9999\n"
              "10 11\n12 13\n",af);
        assert(fclose(af)==0);
        at_terrain_free(&b); assert(at_read_esri_ascii_grid("test-grid-center.asc",&b)==0);
        assert(b.geo.transform[0]==7.5 && b.geo.transform[1]==0.5);
        assert(b.geo.transform[3]==59.0 && b.geo.transform[5]==-0.5);
    }

    /* SRTM/HGT is square signed 16-bit big-endian; -32768 is void. */
    {
        FILE *hf=fopen("N58E007.hgt","wb"); unsigned char hgt[]={
            0x00,0x64, 0x7f,0xff, 0x80,0x00,
            0xff,0xff, 0x00,0x00, 0x00,0x01,
            0x00,0x02, 0x00,0x03, 0x00,0x04
        };
        assert(hf!=NULL); assert(fwrite(hgt,1,sizeof(hgt),hf)==sizeof(hgt)); assert(fclose(hf)==0);
        at_terrain_free(&b); assert(at_read_srtm_hgt("N58E007.hgt",&b)==0);
        assert(b.width==3 && b.height==3);
        assert(b.samples[0]==(uint16_t)(32768+100));
        assert(b.samples[1]==65535);
        assert(b.samples[2]==0);
        assert(b.samples[3]==32767 && b.samples[4]==32768 && b.samples[8]==32772);
        assert(b.geo.valid && b.geo.crs_type==AT_CRS_GEOGRAPHIC && b.geo.coordinate_units==AT_COORD_UNITS_DEGREES);
        assert(b.geo.origin_lat==59.0 && b.geo.origin_lon==7.0);
        assert(b.geo.step_lat==-0.5 && b.geo.step_lon==0.5);
        assert(b.geo.transform[0]==7.0 && b.geo.transform[1]==0.5 && b.geo.transform[3]==59.0 && b.geo.transform[5]==-0.5);
    }

    /* USGS documented void elevation -32767 maps to canonical missing sample 0. */
    {
        FILE *uf=fopen("test-usgs-void.dem","wb"); char a[1024],br[1024],tmp[64];
        assert(uf!=NULL); memset(a,' ',sizeof(a));
        memcpy(a+528,"     2",6);
        snprintf(tmp,sizeof(tmp),"%12.6E",1.0); memcpy(a+816,tmp,12);
        snprintf(tmp,sizeof(tmp),"%12.6E",1.0); memcpy(a+828,tmp,12);
        snprintf(tmp,sizeof(tmp),"%12.6E",1.0); memcpy(a+840,tmp,12);
        memcpy(a+852,"     1",6); memcpy(a+858,"     1",6);
        assert(fwrite(a,1,sizeof(a),uf)==sizeof(a)); memset(br,' ',sizeof(br));
        memcpy(br,"     1",6); memcpy(br+6,"     1",6); memcpy(br+12,"     1",6); memcpy(br+18,"     1",6);
        snprintf(tmp,sizeof(tmp),"%24.15E",0.0); memcpy(br+24,tmp,24);
        snprintf(tmp,sizeof(tmp),"%24.15E",0.0); memcpy(br+48,tmp,24);
        snprintf(tmp,sizeof(tmp),"%24.15E",0.0); memcpy(br+72,tmp,24);
        snprintf(tmp,sizeof(tmp),"%6d",-32767); memcpy(br+144,tmp,6);
        assert(fwrite(br,1,sizeof(br),uf)==sizeof(br)); assert(fclose(uf)==0);
        at_terrain_free(&b); assert(at_read_usgs_dem("test-usgs-void.dem",&b)==0);
        assert(b.width==1 && b.height==1 && b.samples[0]==0);
    }

    /* USGS UTM/NAD83 identity survives import -> ATF -> read. */
    {
        FILE *uf=fopen("test-usgs-utm.dem","wb"); char a[1024],br[1024],tmp[64]; ATTerrain rr={0}; unsigned int y;
        assert(uf!=NULL); memset(a,' ',sizeof(a));
        memcpy(a+156,"     1",6); /* UTM */ memcpy(a+162,"    10",6); /* zone 10 */
        memcpy(a+528,"     2",6); /* metres */ memcpy(a+890," 4",2); /* NAD83 */
        snprintf(tmp,sizeof(tmp),"%12.6E",10.0); memcpy(a+816,tmp,12);
        snprintf(tmp,sizeof(tmp),"%12.6E",20.0); memcpy(a+828,tmp,12);
        snprintf(tmp,sizeof(tmp),"%12.6E",1.0); memcpy(a+840,tmp,12);
        memcpy(a+852,"     1",6); memcpy(a+858,"     1",6);
        assert(fwrite(a,1,sizeof(a),uf)==sizeof(a)); memset(br,' ',sizeof(br));
        memcpy(br,"     1",6); memcpy(br+6,"     1",6); memcpy(br+12,"     3",6); memcpy(br+18,"     1",6);
        snprintf(tmp,sizeof(tmp),"%24s","5.000000000000000D+05"); memcpy(br+24,tmp,24);
        snprintf(tmp,sizeof(tmp),"%24s","4.200000000000000D+06"); memcpy(br+48,tmp,24);
        snprintf(tmp,sizeof(tmp),"%24s","0.000000000000000D+00"); memcpy(br+72,tmp,24);
        for(y=0;y<3;++y) { snprintf(tmp,sizeof(tmp),"%6u",y); memcpy(br+144+6*y,tmp,6); }
        assert(fwrite(br,1,sizeof(br),uf)==sizeof(br)); assert(fclose(uf)==0);
        at_terrain_free(&b); assert(at_read_usgs_dem("test-usgs-utm.dem",&b)==0);
        assert(b.geo.epsg==26910 && b.geo.projection_system==1 && b.geo.projection_zone==10 && b.geo.horizontal_datum==4);
        assert(at_write_atf("test-usgs-utm.atf",&b)==0); assert(at_read_atf("test-usgs-utm.atf",&rr)==0);
        assert(rr.geo.epsg==26910 && rr.geo.projection_system==1 && rr.geo.projection_zone==10 && rr.geo.horizontal_datum==4);
        assert(rr.geo.transform[0]==500000.0 && rr.geo.transform[3]==4200040.0);
        { FILE *af=fopen("test-usgs-utm.atf","rb"); unsigned char buf[512]; size_t an; int saw_geo=0,saw_crs=0; assert(af); an=fread(buf,1,sizeof(buf),af); fclose(af); for(y=0;y+4<=an;++y) { if(!memcmp(buf+y,"GEO ",4)) saw_geo=1; if(!memcmp(buf+y,"CRS ",4)) saw_crs=1; } assert(!saw_geo && saw_crs); }
        at_terrain_free(&rr);
    }

    /* USGS DEM profile spanning more than one 1024-byte logical record. */
    {
        FILE *uf=fopen("test-usgs-long.dem","wb"); char a[1024],bh[144],tmp[64],blank=' '; unsigned int y;
        assert(uf!=NULL); memset(a,' ',sizeof(a)); snprintf(tmp,sizeof(tmp),"%12.6E",1.0); memcpy(a+840,tmp,12); memcpy(a+852,"     1",6); memcpy(a+858,"     1",6);
        assert(fwrite(a,1,sizeof(a),uf)==sizeof(a)); memset(bh,' ',sizeof(bh));
        memcpy(bh,"     1",6); memcpy(bh+6,"     1",6); memcpy(bh+12,"   200",6); memcpy(bh+18,"     1",6);
        snprintf(tmp,sizeof(tmp),"%24s","0.000000000000000D+00"); memcpy(bh+24,tmp,24); memcpy(bh+48,tmp,24); memcpy(bh+72,tmp,24);
        assert(fwrite(bh,1,sizeof(bh),uf)==sizeof(bh));
        for(y=0;y<200;++y) { snprintf(tmp,sizeof(tmp),"%6u",y); assert(fwrite(tmp,1,6,uf)==6); }
        for(y=0;y<704;++y) assert(fwrite(&blank,1,1,uf)==1); /* 1344 bytes -> pad to 2048 */
        assert(fclose(uf)==0); at_terrain_free(&b);
        assert(at_read_usgs_dem("test-usgs-long.dem",&b)==0);
        assert(b.width==1 && b.height==200);
        assert(b.samples[0]==(uint16_t)(199+32768));
        assert(b.samples[199]==32768);
    }

    /* ATF CRS/affine metadata round trip, including rotation terms. */
    {
        ATTerrain g={0},r={0};
        assert(at_terrain_init(&g,2,2)==0);
        g.samples[0]=1; g.samples[1]=2; g.samples[2]=3; g.samples[3]=4;
        g.geo.valid=1; g.geo.elevation_scale=1.0;
        g.geo.crs_type=AT_CRS_PROJECTED; g.geo.coordinate_units=AT_COORD_UNITS_METERS; g.geo.epsg=32632;
        g.geo.projection_system=1; g.geo.projection_zone=32; g.geo.horizontal_datum=4;
        g.geo.transform[0]=500000.0; g.geo.transform[1]=10.0; g.geo.transform[2]=0.25;
        g.geo.transform[3]=6500000.0; g.geo.transform[4]=-0.5; g.geo.transform[5]=-10.0;
        assert(at_write_atf("test-crs.atf",&g)==0);
        assert(at_read_atf("test-crs.atf",&r)==0);
        assert(r.geo.valid && r.geo.crs_type==AT_CRS_PROJECTED);
        assert(r.geo.coordinate_units==AT_COORD_UNITS_METERS && r.geo.epsg==32632);
        assert(r.geo.projection_system==1 && r.geo.projection_zone==32 && r.geo.horizontal_datum==4);
        assert(r.geo.transform[0]==500000.0 && r.geo.transform[1]==10.0 && r.geo.transform[2]==0.25);
        assert(r.geo.transform[3]==6500000.0 && r.geo.transform[4]==-0.5 && r.geo.transform[5]==-10.0);
        at_terrain_free(&g); at_terrain_free(&r);
    }

    /* Geo metadata survives ATF and WCS 1.02 round trips. */
    at_terrain_free(&b);
    a.geo.valid=1; a.geo.origin_lat=58.0; a.geo.origin_lon=7.0;
    a.geo.step_lat=0.01; a.geo.step_lon=0.02; a.geo.elevation_scale=1.0;
    assert(at_write_atf("test-geo.atf",&a)==0);
    assert(at_read_atf("test-geo.atf",&b)==0);
    assert(b.geo.valid && b.geo.origin_lat==58.0 && b.geo.origin_lon==7.0);
    at_terrain_free(&b);
    assert(at_write_wcs_elev("test-wcs-out.elev",&a)==0);
    assert(at_read_wcs_elev("test-wcs-out.elev",&b)==0);
    assert(at_checksum(&b)==at_checksum(&a));
    assert(b.geo.valid && b.geo.origin_lat==58.0 && b.geo.step_lon==0.02);

    at_terrain_free(&a); at_terrain_free(&b);
    remove("test-roundtrip.pgm"); remove("test-roundtrip.atf"); remove("test-vistapro.dem"); remove("test-wcs.elev"); remove("test-geo.atf"); remove("test-wcs-out.elev"); remove("test-vistapro-native.dem"); remove("test-vistapro-truncated.dem"); remove("test.dted"); remove("test-usgs.dem"); remove("test-usgs-long.dem"); remove("test-crs.atf"); remove("test-usgs-projected.dem"); remove("test-usgs-utm.dem"); remove("test-usgs-utm.atf");
    /* ILBM heightmaps: standard planar BODY, uncompressed and ByteRun1. */
    {
        static const unsigned char raw[] = {
            'F','O','R','M',0,0,0,42,'I','L','B','M',
            'B','M','H','D',0,0,0,20, 0,4,0,1,0,0,0,0,1,0,0,0,0,0,10,10,0,4,0,1,
            'B','O','D','Y',0,0,0,2, 0x50,0x00
        };
        FILE *xf=fopen("test-ilbm.iff","wb"); assert(xf!=NULL);
        assert(fwrite(raw,1,sizeof(raw),xf)==sizeof(raw)); assert(fclose(xf)==0);
        at_terrain_free(&b); assert(at_read_ilbm_heightmap("test-ilbm.iff",&b)==0);
        assert(b.width==4 && b.height==1);
        assert(b.samples[0]==0 && b.samples[1]==65535 && b.samples[2]==0 && b.samples[3]==65535);
    }
    {
        static const unsigned char packed[] = {
            'F','O','R','M',0,0,0,44,'I','L','B','M',
            'B','M','H','D',0,0,0,20, 0,4,0,1,0,0,0,0,1,0,1,0,0,0,10,10,0,4,0,1,
            'B','O','D','Y',0,0,0,3, 1,0xA0,0x00,0
        };
        FILE *xf=fopen("test-ilbm-rle.iff","wb"); assert(xf!=NULL);
        assert(fwrite(packed,1,sizeof(packed),xf)==sizeof(packed)); assert(fclose(xf)==0);
        at_terrain_free(&b); assert(at_read_ilbm_heightmap("test-ilbm-rle.iff",&b)==0);
        assert(b.width==4 && b.height==1);
        assert(b.samples[0]==65535 && b.samples[1]==0 && b.samples[2]==65535 && b.samples[3]==0);
    }

    /* 8-plane ILBM reconstructs the classic Amiga planar pixel value. */
    {
        unsigned char raw[12+8+20+8+16]; size_t o=0,p;
        memcpy(raw+o,"FORM",4); o+=4; raw[o++]=0;raw[o++]=0;raw[o++]=0;raw[o++]=56;
        memcpy(raw+o,"ILBM",4); o+=4; memcpy(raw+o,"BMHD",4); o+=4;
        raw[o++]=0;raw[o++]=0;raw[o++]=0;raw[o++]=20;
        { unsigned char bh[20]={0,1,0,1,0,0,0,0,8,0,0,0,0,0,10,10,0,1,0,1}; memcpy(raw+o,bh,20); o+=20; }
        memcpy(raw+o,"BODY",4); o+=4; raw[o++]=0;raw[o++]=0;raw[o++]=0;raw[o++]=16;
        for(p=0;p<8;++p){ raw[o++]=(0xA5U&(1U<<p))?0x80:0; raw[o++]=0; }
        { FILE *xf=fopen("test-ilbm-8bit.ilbm","wb"); assert(xf!=NULL); assert(fwrite(raw,1,o,xf)==o); assert(fclose(xf)==0); }
        at_terrain_free(&b); assert(at_read_ilbm_heightmap("test-ilbm-8bit.ilbm",&b)==0);
        assert(b.width==1 && b.height==1 && b.samples[0]==(uint16_t)(0xA5U*257U));
    }

    /* ILBM writer round-trip is exact at the 8-bit interchange precision. */
    {
        ATTerrain q={0},r={0}; uint32_t i; static const unsigned char values[]={0,1,2,127,128,254,255};
        assert(at_terrain_init(&q,(uint32_t)sizeof(values),1)==0);
        for(i=0;i<(uint32_t)sizeof(values);++i) q.samples[i]=(uint16_t)((uint16_t)values[i]*257U);
        assert(at_write_ilbm_heightmap("test-ilbm-roundtrip.ilbm",&q)==0);
        assert(at_read_ilbm_heightmap("test-ilbm-roundtrip.ilbm",&r)==0);
        assert(r.width==q.width && r.height==q.height);
        for(i=0;i<(uint32_t)sizeof(values);++i) assert(r.samples[i]==q.samples[i]);
        at_terrain_free(&q); at_terrain_free(&r);
    }

    /* ILBM writer emits a standard 256-entry grayscale CMAP. */
    {
        ATTerrain q={0}; FILE *xf; unsigned char buf[820]; size_t n,i,off=(size_t)-1;
        assert(at_terrain_init(&q,1,1)==0); q.samples[0]=32768;
        assert(at_write_ilbm_heightmap("test-ilbm-cmap.ilbm",&q)==0); at_terrain_free(&q);
        xf=fopen("test-ilbm-cmap.ilbm","rb"); assert(xf!=NULL); n=fread(buf,1,sizeof(buf),xf); fclose(xf);
        for(i=0;i+8<n;++i) if(!memcmp(buf+i,"CMAP",4)){ off=i; break; }
        assert(off!=(size_t)-1 && off+8U+768U<=n);
        assert(buf[off+4]==0 && buf[off+5]==0 && buf[off+6]==3 && buf[off+7]==0);
        assert(buf[off+8]==0 && buf[off+9]==0 && buf[off+10]==0);
        assert(buf[off+8+3*127]==127 && buf[off+8+3*127+1]==127 && buf[off+8+3*127+2]==127);
        assert(buf[off+8+3*255]==255 && buf[off+8+3*255+1]==255 && buf[off+8+3*255+2]==255);
    }

    /* CAMG HAM/EHB display modes are not silently treated as linear heights. */
    {
        static const unsigned char ham[] = {
            'F','O','R','M',0,0,0,54,'I','L','B','M',
            'B','M','H','D',0,0,0,20, 0,1,0,1,0,0,0,0,1,0,0,0,0,0,10,10,0,1,0,1,
            'C','A','M','G',0,0,0,4, 0,0,8,0,
            'B','O','D','Y',0,0,0,2, 0,0
        };
        FILE *xf=fopen("test-ilbm-ham.iff","wb"); assert(xf!=NULL);
        assert(fwrite(ham,1,sizeof(ham),xf)==sizeof(ham)); assert(fclose(xf)==0);
        at_terrain_free(&b); assert(at_read_ilbm_heightmap("test-ilbm-ham.iff",&b)!=0);
    }
    {
        static const unsigned char ehb[] = {
            'F','O','R','M',0,0,0,54,'I','L','B','M',
            'B','M','H','D',0,0,0,20, 0,1,0,1,0,0,0,0,1,0,0,0,0,0,10,10,0,1,0,1,
            'C','A','M','G',0,0,0,4, 0,0,0,128,
            'B','O','D','Y',0,0,0,2, 0,0
        };
        FILE *xf=fopen("test-ilbm-ehb.iff","wb"); assert(xf!=NULL);
        assert(fwrite(ehb,1,sizeof(ehb),xf)==sizeof(ehb)); assert(fclose(xf)==0);
        at_terrain_free(&b); assert(at_read_ilbm_heightmap("test-ilbm-ehb.iff",&b)!=0);
    }

    /* CMAP is display metadata: planar pixel index remains the height value. */
    {
        static const unsigned char pal[] = {
            'F','O','R','M',0,0,0,56,'I','L','B','M',
            'B','M','H','D',0,0,0,20, 0,1,0,1,0,0,0,0,1,0,0,0,0,0,10,10,0,1,0,1,
            'C','M','A','P',0,0,0,6, 255,0,0, 0,255,0,
            'B','O','D','Y',0,0,0,2, 0x80,0
        };
        FILE *xf=fopen("test-ilbm-colour-cmap.iff","wb"); assert(xf!=NULL);
        assert(fwrite(pal,1,sizeof(pal),xf)==sizeof(pal)); assert(fclose(xf)==0);
        at_terrain_free(&b); assert(at_read_ilbm_heightmap("test-ilbm-colour-cmap.iff",&b)==0);
        assert(b.width==1 && b.height==1 && b.samples[0]==65535);
    }

    /* ILBM mask plane is display-only: both pixels retain index-derived heights. */
    {
        static const unsigned char masked[] = {
            'F','O','R','M',0,0,0,44,'I','L','B','M',
            'B','M','H','D',0,0,0,20, 0,2,0,1,0,0,0,0,1,1,0,0,0,0,10,10,0,2,0,1,
            'B','O','D','Y',0,0,0,4, 0xC0,0, 0x80,0
        };
        FILE *xf=fopen("test-ilbm-mask.iff","wb"); assert(xf!=NULL);
        assert(fwrite(masked,1,sizeof(masked),xf)==sizeof(masked)); assert(fclose(xf)==0);
        at_terrain_free(&b); assert(at_read_ilbm_heightmap("test-ilbm-mask.iff",&b)==0);
        assert(b.width==2 && b.height==1);
        assert(b.samples[0]==65535 && b.samples[1]==65535);
    }
    /* Truncated BODY must fail rather than read outside the available data. */
    {
        static const unsigned char truncated[] = {
            'F','O','R','M',0,0,0,42,'I','L','B','M',
            'B','M','H','D',0,0,0,20, 0,1,0,1,0,0,0,0,1,0,0,0,0,0,10,10,0,1,0,1,
            'B','O','D','Y',0,0,0,2, 0x80
        };
        FILE *xf=fopen("test-ilbm-truncated.iff","wb"); assert(xf!=NULL);
        assert(fwrite(truncated,1,sizeof(truncated),xf)==sizeof(truncated)); assert(fclose(xf)==0);
        at_terrain_free(&b); assert(at_read_ilbm_heightmap("test-ilbm-truncated.iff",&b)!=0);
    }

    /* Malformed IFF chunk: odd payload without mandatory pad byte. */
    {
        static const unsigned char badpad[] = {
            'F','O','R','M',0,0,0,45,'I','L','B','M',
            'B','M','H','D',0,0,0,20, 0,1,0,1,0,0,0,0,1,0,0,0,0,0,10,10,0,1,0,1,
            'J','U','N','K',0,0,0,1, 42,
            'B','O','D','Y',0,0,0,2, 0x80,0
        };
        FILE *xf=fopen("test-ilbm-badpad.iff","wb"); assert(xf);
        assert(fwrite(badpad,1,sizeof(badpad),xf)==sizeof(badpad)); assert(fclose(xf)==0);
        at_terrain_free(&b); assert(at_read_ilbm_heightmap("test-ilbm-badpad.iff",&b)!=0);
    }
    /* ByteRun1 repeat command with missing repeated byte must fail. */
    {
        static const unsigned char badrle[] = {
            'F','O','R','M',0,0,0,42,'I','L','B','M',
            'B','M','H','D',0,0,0,20, 0,1,0,1,0,0,0,0,1,0,1,0,0,0,10,10,0,1,0,1,
            'B','O','D','Y',0,0,0,2, 0xff
        };
        FILE *xf=fopen("test-ilbm-badrle.iff","wb"); assert(xf);
        assert(fwrite(badrle,1,sizeof(badrle),xf)==sizeof(badrle)); assert(fclose(xf)==0);
        at_terrain_free(&b); assert(at_read_ilbm_heightmap("test-ilbm-badrle.iff",&b)!=0);
    }
    /* Declared FORM length cannot end halfway through a chunk header. */
    {
        static const unsigned char partial[] = {
            'F','O','R','M',0,0,0,7,'I','L','B','M', 'B','M','H'
        };
        FILE *xf=fopen("test-ilbm-partial-form.iff","wb"); assert(xf);
        assert(fwrite(partial,1,sizeof(partial),xf)==sizeof(partial)); assert(fclose(xf)==0);
        at_terrain_free(&b); assert(at_read_ilbm_heightmap("test-ilbm-partial-form.iff",&b)!=0);
    }

    /* BODY length must constrain decoding even if following bytes exist. */
    {
        static const unsigned char shortbody[] = {
            'F','O','R','M',0,0,0,48,'I','L','B','M',
            'B','M','H','D',0,0,0,20, 0,1,0,1,0,0,0,0,1,0,0,0,0,0,10,10,0,1,0,1,
            'B','O','D','Y',0,0,0,1, 0x80,0,
            'J','U','N','K',0,0,0,0
        };
        FILE *xf=fopen("test-ilbm-short-body.iff","wb"); assert(xf);
        assert(fwrite(shortbody,1,sizeof(shortbody),xf)==sizeof(shortbody)); assert(fclose(xf)==0);
        at_terrain_free(&b); assert(at_read_ilbm_heightmap("test-ilbm-short-body.iff",&b)!=0);
    }

    /* ByteRun1 literal must not consume bytes from the next IFF chunk. */
    {
        static const unsigned char short_rle[] = {
            'F','O','R','M',0,0,0,48,'I','L','B','M',
            'B','M','H','D',0,0,0,20, 0,1,0,1,0,0,0,0,1,0,1,0,0,0,10,10,0,1,0,1,
            'B','O','D','Y',0,0,0,1, 1,0,
            'J','U','N','K',0,0,0,0
        };
        FILE *xf=fopen("test-ilbm-rle-body-bound.iff","wb"); assert(xf);
        assert(fwrite(short_rle,1,sizeof(short_rle),xf)==sizeof(short_rle)); assert(fclose(xf)==0);
        at_terrain_free(&b); assert(at_read_ilbm_heightmap("test-ilbm-rle-body-bound.iff",&b)!=0);
    }

    /* ByteRun1 -128 is a NOP; a following repeat fills one planar row. */
    {
        static const unsigned char nop_rle[] = {
            'F','O','R','M',0,0,0,44,'I','L','B','M',
            'B','M','H','D',0,0,0,20, 0,1,0,1,0,0,0,0,1,0,1,0,0,0,10,10,0,1,0,1,
            'B','O','D','Y',0,0,0,4, 0x80,0xff,0x80,0
        };
        FILE *xf=fopen("test-ilbm-rle-nop.iff","wb"); assert(xf);
        assert(fwrite(nop_rle,1,sizeof(nop_rle),xf)==sizeof(nop_rle)); assert(fclose(xf)==0);
        at_terrain_free(&b); assert(at_read_ilbm_heightmap("test-ilbm-rle-nop.iff",&b)==0);
        assert(b.width==1 && b.height==1 && b.samples[0]==65535);
    }
    /* A repeat opcode cannot fetch its value from the next chunk. */
    {
        static const unsigned char repeat_bound[] = {
            'F','O','R','M',0,0,0,48,'I','L','B','M',
            'B','M','H','D',0,0,0,20, 0,1,0,1,0,0,0,0,1,0,1,0,0,0,10,10,0,1,0,1,
            'B','O','D','Y',0,0,0,1, 0xff,0,
            'J','U','N','K',0,0,0,0
        };
        FILE *xf=fopen("test-ilbm-rle-repeat-bound.iff","wb"); assert(xf);
        assert(fwrite(repeat_bound,1,sizeof(repeat_bound),xf)==sizeof(repeat_bound)); assert(fclose(xf)==0);
        at_terrain_free(&b); assert(at_read_ilbm_heightmap("test-ilbm-rle-repeat-bound.iff",&b)!=0);
    }

    /* A second BODY chunk must not override the first. */
    {
        static const unsigned char duplicate_body[] = {
            'F','O','R','M',0,0,0,54,'I','L','B','M',
            'B','M','H','D',0,0,0,20, 0,1,0,1,0,0,0,0,1,0,0,0,0,0,10,10,0,1,0,1,
            'B','O','D','Y',0,0,0,2, 0x80,0,
            'B','O','D','Y',0,0,0,2, 0,0
        };
        FILE *xf=fopen("test-ilbm-duplicate-body.iff","wb"); assert(xf);
        assert(fwrite(duplicate_body,1,sizeof(duplicate_body),xf)==sizeof(duplicate_body)); assert(fclose(xf)==0);
        at_terrain_free(&b); assert(at_read_ilbm_heightmap("test-ilbm-duplicate-body.iff",&b)!=0);
    }

    puts("core tests: PASS");
    return 0;
}
