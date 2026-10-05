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
        assert(b.samples[0]==(uint16_t)(357+32768));
        assert(b.samples[257]==(uint16_t)(357+257+32768));
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
    remove("test-roundtrip.pgm"); remove("test-roundtrip.atf"); remove("test-vistapro.dem"); remove("test-wcs.elev"); remove("test-geo.atf"); remove("test-wcs-out.elev"); remove("test-vistapro-native.dem");
    puts("core tests: PASS");
    return 0;
}
