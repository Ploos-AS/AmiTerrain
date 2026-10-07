#ifndef AMITERRAIN_H
#define AMITERRAIN_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    AT_CRS_UNKNOWN = 0,
    AT_CRS_GEOGRAPHIC = 1,
    AT_CRS_PROJECTED = 2
} ATCRSType;

typedef enum {
    AT_COORD_UNITS_UNKNOWN = 0,
    AT_COORD_UNITS_DEGREES = 1,
    AT_COORD_UNITS_METERS = 2,
    AT_COORD_UNITS_FEET = 3
} ATCoordinateUnits;

typedef struct {
    int valid;
    /* v1 compatibility view for geographic north-up rasters. */
    double origin_lat;
    double origin_lon;
    double step_lat;
    double step_lon;
    double elevation_scale;

    /* v2 generic spatial model.
       World coordinate of pixel (col,row):
       X = transform[0] + col*transform[1] + row*transform[2]
       Y = transform[3] + col*transform[4] + row*transform[5] */
    ATCRSType crs_type;
    ATCoordinateUnits coordinate_units;
    int32_t epsg;
    int32_t projection_system; /* source format code when known, else 0 */
    int32_t projection_zone;   /* source zone when known, else 0 */
    int32_t horizontal_datum;  /* source datum code when known, else 0 */
    double transform[6];
} ATGeoMetadata;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint16_t *samples;
    ATGeoMetadata geo;
} ATTerrain;

int at_terrain_init(ATTerrain *terrain, uint32_t width, uint32_t height);
void at_terrain_free(ATTerrain *terrain);
void at_generate_noise(ATTerrain *terrain, uint32_t seed);
uint32_t at_checksum(const ATTerrain *terrain);

int at_write_pgm16(const char *path, const ATTerrain *terrain);
int at_read_pgm16(const char *path, ATTerrain *terrain);
int at_write_raw16be(const char *path, const ATTerrain *terrain);
int at_read_raw16be(const char *path, uint32_t width, uint32_t height, ATTerrain *terrain);
int at_write_atf(const char *path, const ATTerrain *terrain);
int at_read_atf(const char *path, ATTerrain *terrain);
int at_read_vistapro_binary(const char *path, uint32_t width, uint32_t height, ATTerrain *terrain);
int at_write_vistapro_binary(const char *path, const ATTerrain *terrain);
int at_read_vistapro_dem(const char *path, ATTerrain *terrain);
int at_read_dted(const char *path, ATTerrain *terrain);
int at_read_usgs_dem(const char *path, ATTerrain *terrain);
int at_read_srtm_hgt(const char *path, ATTerrain *terrain);
int at_read_esri_ascii_grid(const char *path, ATTerrain *terrain);
int at_read_xyz_grid(const char *path, ATTerrain *terrain);
int at_read_ilbm_heightmap(const char *path, ATTerrain *terrain);
int at_read_wcs_elev(const char *path, ATTerrain *terrain);
int at_write_wcs_elev(const char *path, const ATTerrain *terrain);

#endif
