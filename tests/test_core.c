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
        hdr[131]=1; /* compression != 0 */
        hdr[138]=1; hdr[139]=2; /* 258 BE */
        hdr[142]=1; hdr[143]=2;
        assert(fwrite(hdr,1,sizeof(hdr),vf)==sizeof(hdr));
        for(row=0;row<258;++row) {
            unsigned char raw[259], packed[262]; size_t p=0;
            int16_t base=(int16_t)(100+(int)row);
            raw[0]=(unsigned char)(((uint16_t)base)>>8); raw[1]=(unsigned char)base;
            for(x=2;x<259;++x) raw[x]=1; /* monotonic deltas */
            if(row==0) { /* force VistaPro's -128 escape/new-base path */
                raw[2]=0x80; raw[3]=0x01; raw[4]=0xf4; /* new base = 500 */
                for(x=5;x<259;++x) raw[x]=0;
            }
            packed[p++]=127; memcpy(packed+p,raw,128); p+=128;
            packed[p++]=127; memcpy(packed+p,raw+128,128); p+=128;
            packed[p++]=2; memcpy(packed+p,raw+256,3); p+=3;
            assert(fputc((int)(p>>8),vf)!=EOF); assert(fputc((int)(p&255),vf)!=EOF);
            assert(fwrite(packed,1,p,vf)==p);
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
        snprintf(tmp,sizeof(tmp),"%24s","1.000000000000000D+01"); memcpy(a+816,tmp,24);
        snprintf(tmp,sizeof(tmp),"%24s","2.000000000000000D+01"); memcpy(a+840,tmp,24);
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
    }

    /* USGS DEM profile spanning more than one 1024-byte logical record. */
    {
        FILE *uf=fopen("test-usgs-long.dem","wb"); char a[1024],bh[144],tmp[64],blank=' '; unsigned int y;
        assert(uf!=NULL); memset(a,' ',sizeof(a)); memcpy(a+852,"     1",6); memcpy(a+858,"     1",6);
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
        g.geo.transform[0]=500000.0; g.geo.transform[1]=10.0; g.geo.transform[2]=0.25;
        g.geo.transform[3]=6500000.0; g.geo.transform[4]=-0.5; g.geo.transform[5]=-10.0;
        assert(at_write_atf("test-crs.atf",&g)==0);
        assert(at_read_atf("test-crs.atf",&r)==0);
        assert(r.geo.valid && r.geo.crs_type==AT_CRS_PROJECTED);
        assert(r.geo.coordinate_units==AT_COORD_UNITS_METERS && r.geo.epsg==32632);
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
    remove("test-roundtrip.pgm"); remove("test-roundtrip.atf"); remove("test-vistapro.dem"); remove("test-wcs.elev"); remove("test-geo.atf"); remove("test-wcs-out.elev"); remove("test-vistapro-native.dem"); remove("test-vistapro-truncated.dem"); remove("test.dted"); remove("test-usgs.dem"); remove("test-usgs-long.dem"); remove("test-crs.atf"); remove("test-usgs-projected.dem");
    puts("core tests: PASS");
    return 0;
}
