#include "amiterrain.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    ATTerrain a = {0,0,0}, b = {0,0,0};
    uint32_t ca, cb;

    assert(at_terrain_init(&a, 32, 32) == 0);
    assert(at_terrain_init(&b, 32, 32) == 0);
    at_generate_noise(&a, 6502);
    at_generate_noise(&b, 6502);
    ca = at_checksum(&a);
    cb = at_checksum(&b);
    assert(ca == cb);

    assert(at_write_pgm16("test-roundtrip.pgm", &a) == 0);
    at_terrain_free(&b);
    assert(at_read_pgm16("test-roundtrip.pgm", &b) == 0);
    assert(at_checksum(&b) == ca);

    at_terrain_free(&a);
    at_terrain_free(&b);
    remove("test-roundtrip.pgm");
    puts("core tests: PASS");
    return 0;
}
