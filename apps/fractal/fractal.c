// FRACTAL - Mandelbrot and Julia sets in 80x24 text, rendered progressively.
//
// Escape-time is computed in x87 `double`, which needs no libm (we compare
// |z|^2 > 4 instead of taking a square root). Each cell gets its own colour.
//
//   arrows  pan           + / -  zoom in / out      j  toggle Julia
//   1-4     Julia presets (in Julia mode)           r  reset view
//   q/ESC   quit
//
// Build: ./build.sh -> dist/FRACTAL.BIN

__asm__(".code16gcc\n");
#include "api.h"
#include "ogfx.h"

#define H 24              /* rows 0..23; row 24 is the status line */
#define MAXIT 80

static double cx, cy, sx, sy;
static uint8_t julia;
static double jcr, jci;
static uint8_t bg;

static const uint8_t pal[15] = {
    9, 11, 14, 10, 12, 13, 15, 7, 3, 2, 6, 4, 5, 8, 1
};
static const char framp[] = ".:-=+*#%@";

/* Returns iteration count, or MAXIT if the point stayed bounded. */
static int escape(double x0, double y0, int maxit) {
    double zr, zi, cr, ci;
    if (julia) { zr = x0; zi = y0; cr = jcr; ci = jci; }
    else       { zr = 0;  zi = 0;  cr = x0;  ci = y0;  }

    int i;
    for (i = 0; i < maxit; i++) {
        double zr2 = zr * zr;
        double zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) return i;
        zi = 2.0 * zr * zi + ci;
        zr = zr2 - zi2 + cr;
    }
    return maxit;
}

static void render(int maxit) {
    for (int r = 0; r < H; r++) {
        double y = cy + sy - (2.0 * sy) * r / (double)(H - 1);
        for (int c = 0; c < 80; c++) {
            double x = cx - sx + (2.0 * sx) * c / 79.0;
            int it = escape(x, y, maxit);

            char ch;
            uint8_t attr;
            if (it >= maxit) { ch = ' '; attr = bg; }
            else {
                int idx = (it * 8) / maxit;
                ch = framp[idx];
                attr = (uint8_t)(bg | pal[it % 15]);
            }
            o_poke((uint8_t)c, (uint8_t)r, ch, attr);
        }
    }
}

static void print_i16(int v) {
    if (v < 0) { print_char('-'); v = -v; }
    print_int((uint16_t)v);
}

static void status(void) {
    o_fill(0, 24, 80, 1, ' ', bg);
    gotoxy(0, 24);
    print_str(julia ? "Julia " : "Mandel");
    print_str("  center ");
    print_i16((int)(cx * 1000));
    print_str(",");
    print_i16((int)(cy * 1000));
    print_str("  span ");
    print_i16((int)(sx * 1000));
    print_str("  +/- zoom  arrows pan  j julia  r reset  q quit");
}

static void reset_view(void) {
    if (julia) { cx = 0; cy = 0; sx = 1.6; sy = 1.0; }
    else       { cx = -0.6; cy = 0; sx = 2.0; sy = 1.25; }
}

static void draw_progressive(void) {
    static const int passes[5] = { 8, 16, 32, 64, MAXIT };
    for (int p = 0; p < 5; p++) render(passes[p]);
    status();
}

void module_main(void) {
    bg = (uint8_t)(get_cur_col() & 0xF0);
    julia = 0;
    jcr = -0.8; jci = 0.156;
    reset_view();

    clear_screen();
    draw_progressive();

    for (;;) {
        uint16_t k = get_key();
        uint8_t ch = (uint8_t)(k & 0xFF);
        uint8_t sc = (uint8_t)(k >> 8);
        double stepx = sx * 0.12;
        double stepy = sy * 0.12;
        int redraw = 1;

        if (ch == 0x1B || ch == 'q' || ch == 'Q') break;

        if (sc == 0x48)      cy -= stepy;      /* up    */
        else if (sc == 0x50) cy += stepy;      /* down  */
        else if (sc == 0x4B) cx -= stepx;      /* left  */
        else if (sc == 0x4D) cx += stepx;      /* right */
        else if (ch == '+' || ch == '=') { sx *= 0.8; sy *= 0.8; }
        else if (ch == '-' || ch == '_') { sx *= 1.25; sy *= 1.25; }
        else if (ch == 'j' || ch == 'J') { julia = (uint8_t)!julia; reset_view(); }
        else if (ch == 'r' || ch == 'R') reset_view();
        else if (julia && ch == '1') { jcr = -0.8;    jci = 0.156; }
        else if (julia && ch == '2') { jcr = -0.7269; jci = 0.1889; }
        else if (julia && ch == '3') { jcr = 0.285;   jci = 0.01; }
        else if (julia && ch == '4') { jcr = -0.4;    jci = 0.6; }
        else redraw = 0;

        if (redraw) draw_progressive();
    }

    clear_screen();
}
