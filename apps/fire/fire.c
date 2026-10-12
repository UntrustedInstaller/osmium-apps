// FIRE - flickering ASCII flame, aafire-style.
//
// Heat rises: the bottom row is re-seeded with random fuel, every row above is
// a smoothed, slightly cooler copy of the row below, and each cell's heat maps
// to a glyph and a red/orange/yellow colour. Forced black background.
// ESC (or q) quits.
//
// Build: ./build.sh -> dist/FIRE.BIN

__asm__(".code16gcc\n");
#include "api.h"
#include "ogfx.h"
#include "ostr.h"
#include "osnd.h"
#include "outil.h"

#define W 80
#define H 25

static uint8_t heat[H][W];

static char fire_char(uint8_t h) {
    return O_RAMP[(uint16_t)h * (O_RAMP_LEN - 1) / 255];
}

static uint8_t fire_color(uint8_t h) {
    if (h >= 224) return 0x0F;   /* white hot */
    if (h >= 176) return 0x0E;   /* yellow    */
    if (h >= 112) return 0x06;   /* orange    */
    if (h >= 48)  return 0x0C;   /* light red */
    if (h >= 16)  return 0x04;   /* red       */
    return 0x00;
}

static void step(void) {
    /* Re-seed the fuel row, with a few 255 hotspots for tongues. */
    for (int c = 0; c < W; c++)
        heat[H - 1][c] = (uint8_t)o_rng();
    for (int i = 0; i < 8; i++)
        heat[H - 1][o_randn(W)] = 255;

    /* Each row averages the one below (centre-weighted) and cools a little. */
    for (int r = H - 2; r >= 0; r--) {
        for (int c = 0; c < W; c++) {
            int l  = c > 0     ? heat[r + 1][c - 1] : heat[r + 1][c];
            int m  = heat[r + 1][c];
            int rr = c < W - 1 ? heat[r + 1][c + 1] : heat[r + 1][c];
            int v = (l + 2 * m + rr) >> 2;
            v -= 9;
            heat[r][c] = v > 0 ? (uint8_t)v : 0;
        }
    }
}

void module_main(void) {
    o_seed_rng();
    clear_screen();

    for (int r = 0; r < H; r++)
        for (int c = 0; c < W; c++)
            heat[r][c] = 0;

    for (;;) {
        step();

        for (int r = 0; r < H; r++)
            for (int c = 0; c < W; c++) {
                uint8_t h = heat[r][c];
                o_poke((uint8_t)c, (uint8_t)r, fire_char(h), fire_color(h));
            }

        if (o_kbhit()) {
            uint16_t k = get_key();
            uint8_t ch = (uint8_t)(k & 0xFF);
            if (ch == 0x1B || ch == 'q' || ch == 'Q') break;
        }
        o_delay_ms(40);
    }

    clear_screen();
}
