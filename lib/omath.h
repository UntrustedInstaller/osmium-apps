#ifndef OSM_OMATH_H
#define OSM_OMATH_H

#include "api.h"

/* A 64-entry sine table covering one full period (index 0..63 -> 0..2*pi).
 * Values are sin(2*pi*i/64) scaled to -127..127. Because `int` is 16-bit
 * under -m16, keep callers' arithmetic inside that range. */
static const signed char o_sin64[64] = {
      0,  12,  25,  37,  49,  60,  71,  81,
     90,  98, 106, 112, 118, 122, 125, 127,
    127, 127, 125, 122, 118, 112, 106,  98,
     90,  81,  71,  60,  49,  37,  25,  12,
      0, -12, -25, -37, -49, -60, -71, -81,
    -90, -98,-106,-112,-118,-122,-125,-127,
   -127,-127,-125,-122,-118,-112,-106, -98,
    -90, -81, -71, -60, -49, -37, -25, -12
};

/* phase is a 0..63 index into one period. */
static inline int o_sin(uint8_t phase) { return (int)o_sin64[phase & 63]; }
static inline int o_cos(uint8_t phase) { return (int)o_sin64[(uint8_t)(phase + 16) & 63]; }

#endif
