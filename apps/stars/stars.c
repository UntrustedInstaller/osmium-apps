// STARS - 3D starfield / warp-tunnel screensaver.
//
// Stars live in a box that is projected to the screen and divided by their
// depth, so they stream out of a point in the middle. Each is drawn as a
// short streak that brightens as it nears the viewer, then respawns far away.
// Forced black background. ESC (or q) quits.
//
// Build: ./build.sh -> dist/STARS.BIN

__asm__(".code16gcc\n");
#include "api.h"
#include "ogfx.h"
#include "ostr.h"
#include "osnd.h"
#include "outil.h"

#define W 80
#define H 25
#define NSTARS 84
#define CX 40            /* projection centre, columns */
#define CY 12            /* projection centre, rows    */
#define ZFAR 1024
#define ZSTEP 6

static int sx[NSTARS], sy[NSTARS], sz[NSTARS];
static int px[NSTARS], py[NSTARS];   /* previous cell, -1 = unset */

static void respawn(int i) {
    sx[i] = (int)o_randn(513) - 256;
    sy[i] = (int)o_randn(513) - 256;
    sz[i] = ZFAR - (int)o_randn(320);
    px[i] = -1;
    py[i] = -1;
}

static uint8_t star_color(int z) {
    if (z > 700) return 0x08;   /* dim grey   */
    if (z > 380) return 0x07;   /* light grey */
    if (z > 160) return 0x0B;   /* cyan       */
    return 0x0F;                /* white      */
}

static char star_char(int z) {
    if (z > 700) return '.';
    if (z > 380) return '+';
    return '*';
}

static void draw_streak(int ox, int oy, int nx, int ny, char c, uint8_t attr) {
    if (ox < 0 || oy < 0) {
        if (nx >= 0 && nx < W && ny >= 0 && ny < H)
            o_poke((uint8_t)nx, (uint8_t)ny, c, attr);
        return;
    }
    int dx = nx - ox, dy = ny - oy;
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    int steps = adx > ady ? adx : ady;
    if (steps < 1) steps = 1;
    if (steps > 24) steps = 24;      /* clamp the near-viewer jump */
    for (int s = 0; s <= steps; s++) {
        int x = ox + dx * s / steps;
        int y = oy + dy * s / steps;
        if (x >= 0 && x < W && y >= 0 && y < H)
            o_poke((uint8_t)x, (uint8_t)y, c, attr);
    }
}

void module_main(void) {
    o_seed_rng();
    clear_screen();

    for (int i = 0; i < NSTARS; i++) respawn(i);

    for (;;) {
        o_fill(0, 0, W, H, ' ', 0x00);

        for (int i = 0; i < NSTARS; i++) {
            int z = sz[i];
            if (z <= 4) { respawn(i); continue; }

            int nx = CX + (sx[i] * CX) / z;
            int ny = CY + (sy[i] * (CY + 1)) / z;

            draw_streak(px[i], py[i], nx, ny, star_char(z), star_color(z));

            px[i] = nx;
            py[i] = ny;
            sz[i] = z - ZSTEP;
        }

        if (o_kbhit()) {
            uint16_t k = get_key();
            uint8_t ch = (uint8_t)(k & 0xFF);
            if (ch == 0x1B || ch == 'q' || ch == 'Q') break;
        }
        o_delay_ms(38);
    }

    clear_screen();
}
