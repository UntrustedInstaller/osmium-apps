// MATRIX - falling-glyph rain screensaver, cmatrix-style.
//
// Every column owns a drop that falls at its own speed. A cell's brightness
// decays one step per frame, so each drop leaves a fading green trail behind a
// white-hot head. The background is forced black. ESC (or q) quits.
//
// Build: ./build.sh -> dist/MATRIX.BIN

__asm__(".code16gcc\n");
#include "api.h"
#include "ogfx.h"
#include "ostr.h"
#include "osnd.h"
#include "outil.h"

#define W 80
#define H 25
#define MAXLEV 12

static uint8_t lev[H][W];    /* per-cell brightness, 0 = off   */
static char    glyph[H][W];  /* glyph shown in each cell       */
static uint8_t head[W];      /* head row per column, 0xFF=idle */
static uint8_t spd[W];       /* frames between two steps       */
static uint8_t cnt[W];       /* frame counter per column       */

static const char pool[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@#$%&*+-=<>/\\|^~";

/* Brightness -> attribute. Head is white, then it cools through two greens
 * into a dark grey before going out. */
static uint8_t shade(uint8_t l) {
    if (l == 0)  return 0x00;
    if (l >= 11) return 0x0F;   /* white head  */
    if (l >= 8)  return 0x0A;   /* light green */
    if (l >= 4)  return 0x02;   /* green       */
    return 0x08;                /* dark grey   */
}

void module_main(void) {
    o_seed_rng();
    clear_screen();
    o_fill(0, 0, W, H, ' ', 0x00);

    for (int c = 0; c < W; c++) {
        head[c] = 0xFF;
        if ((o_rng() & 7) == 0) {          /* stagger the opening shower */
            head[c] = 0;
            spd[c] = (uint8_t)(1 + o_randn(3));
            cnt[c] = (uint8_t)o_randn(3);
        }
    }

    for (;;) {
        /* Cool every lit cell one step; paint the result. */
        for (int r = 0; r < H; r++)
            for (int c = 0; c < W; c++)
                if (lev[r][c]) {
                    lev[r][c]--;
                    o_poke((uint8_t)c, (uint8_t)r,
                           lev[r][c] ? glyph[r][c] : ' ', shade(lev[r][c]));
                }

        /* Move/stage the drops. */
        for (int c = 0; c < W; c++) {
            if (head[c] == 0xFF) {
                if (o_randn(100) < 6) {    /* start a fresh drop */
                    head[c] = 0;
                    spd[c] = (uint8_t)(1 + o_randn(3));
                    cnt[c] = 0;
                }
                continue;
            }
            if (++cnt[c] < spd[c]) continue;
            cnt[c] = 0;

            if (head[c] < H) {
                glyph[head[c]][c] = pool[o_randn((uint16_t)(sizeof(pool) - 1))];
                lev[head[c]][c] = MAXLEV;
                o_poke((uint8_t)c, head[c], glyph[head[c]][c], shade(MAXLEV));
            }
            /* Let the drop run a little past the bottom before recycling. */
            if (head[c] >= H + (uint8_t)(spd[c] * 4)) head[c] = 0xFF;
            else head[c]++;
        }

        if (o_kbhit()) {
            uint16_t k = get_key();
            uint8_t ch = (uint8_t)(k & 0xFF);
            if (ch == 0x1B || ch == 'q' || ch == 'Q') break;
        }
        o_delay_ms(45);
    }

    clear_screen();
}
