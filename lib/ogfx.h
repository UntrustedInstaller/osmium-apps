#ifndef OSM_OGFX_H
#define OSM_OGFX_H

#include "api.h"

#define O_COLS 80
#define O_ROWS 25

/* CP437 block glyphs, handy for half-resolution and shading tricks. */
#define O_BLOCK_FULL  0xDB
#define O_BLOCK_DARK  0xB2
#define O_BLOCK_MED   0xB1
#define O_BLOCK_LIGHT 0xB0
#define O_BLOCK_UPPER 0xDF  /* upper half block  */
#define O_BLOCK_LOWER 0xDC  /* lower half block  */

/* Luminance ramp, low -> high. */
#define O_RAMP " .:-=+*#%@"
#define O_RAMP_LEN 10

/* 15-step hue ramp: blue -> cyan -> green -> yellow -> red -> magenta -> grey.
 * Meant for escape-time and shade colouring, so it reads as a smooth walk
 * around the colour wheel rather than a random palette. */
static const uint8_t o_hue[15] = {
    1, 9, 11, 3, 10, 2, 14, 6, 12, 4, 13, 5, 15, 7, 8
};

/* Write one cell straight into text-mode VGA at 0xB800:offset.
 * Assumes the default 80x25 alphanumeric mode. */
static inline void o_poke(uint8_t col, uint8_t row, char c, uint8_t attr) {
    uint16_t off = (uint16_t)(((uint16_t)row * 80u + (uint16_t)col) * 2u);
    uint16_t cw  = (uint16_t)(((uint16_t)attr << 8) | (uint8_t)c);
    __asm__ __volatile__(
        "pushw %%es\n\t"
        "movw $0xB800, %%ax\n\t"
        "movw %%ax, %%es\n\t"
        "movw %0, %%di\n\t"
        "movw %1, %%es:(%%di)\n\t"
        "popw %%es\n\t"
        : : "r"(off), "r"(cw) : "ax", "di", "memory"
    );
}

/* Fill a rectangle with one character/attribute. */
static inline void o_fill(uint8_t col, uint8_t row, uint8_t w, uint8_t h,
                          char c, uint8_t attr) {
    uint8_t y, x;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            o_poke((uint8_t)(col + x), (uint8_t)(row + y), c, attr);
}

/* Print a string at a cell with an explicit attribute, bypassing the
 * kernel cursor entirely (so it does not race with the shell's). */
static inline void o_print_at(uint8_t col, uint8_t row, const char* s, uint8_t attr) {
    uint8_t c = col;
    while (*s) o_poke(c++, row, *s++, attr);
}

#endif
