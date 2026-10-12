// DONUT - rotating 3D ASCII torus, after Andy Sloane's donut.c.
//
// The original uses libm sin/cos; here they come straight from the x87 FPU
// (fsin/fcos) so the module links with no libc. ESC or q quits.
//
// Build: ./build.sh -> dist/DONUT.BIN

__asm__(".code16gcc\n");
#include "api.h"
#include "ogfx.h"
#include "ostr.h"
#include "osnd.h"

#define W 80
#define H 22

static inline double dsin(double x) {
    double r; __asm__ __volatile__("fsin" : "=t"(r) : "0"(x)); return r;
}
static inline double dcos(double x) {
    double r; __asm__ __volatile__("fcos" : "=t"(r) : "0"(x)); return r;
}

static char  b[W * H];
static float z[W * H];

static const char ramp[] = ".,-~:;=!*#$@";

/* Bright white on black: monochrome donut. The ASCII ramp above still gives
 * luminance shading within the single foreground colour. */
#define DONUT_FG 0x0F

static int kbhit(void) {
    uint8_t v;
    __asm__ __volatile__("movb $0x01, %%ah\n\tint $0x16\n\tsetnz %0"
                         : "=q"(v) : : "eax");
    return v;
}

void module_main(void) {
    double A = 0, B = 0;
    uint8_t bg = 0x00;              /* screensaver: always black */

    clear_screen();
    /* The torus only fills rows 0..H-1; black out the rest so no shell
     * theme background shows through. */
    o_fill(0, 0, O_COLS, O_ROWS, ' ', bg);

    /* Bottom hint bar (rows H..23 are never redrawn by the torus). */
    o_print_at(0, 24, "ESC / q quit", 0x07);

    for (;;) {
        o_memset(b, ' ', sizeof(b));
        o_memset(z, 0, sizeof(z));

        for (double j = 0; j < 6.28; j += 0.09) {
            double c = dsin(j);
            double d = dcos(j);
            double e = dsin(A);
            double f = dsin(j);
            double g = dcos(A);

            for (double i = 0; i < 6.28; i += 0.03) {
                double cc = dsin(i);
                double h  = d + 2;
                double D  = 1.0 / (cc * h * e + f * g + 5);
                double l  = dcos(i);
                double m  = dcos(B);
                double n  = dsin(B);
                double t  = cc * h * g - f * e;

                int x = (int)(40 + 30 * D * (l * h * m - t * n));
                int y = (int)(12 + 15 * D * (l * h * n + t * m));
                int o = x + 80 * y;
                int N = (int)(8 * ((f * e - cc * d * g) * m - cc * d * e - f * g - l * d * n));

                if (y > 0 && y < 22 && x > 0 && x < 80 && D > z[o]) {
                    z[o] = (float)D;
                    b[o] = ramp[N > 0 ? N : 0];
                }
            }
        }

        for (int r = 0; r < H; r++)
            for (int c2 = 0; c2 < W; c2++) {
                char ch = b[r * W + c2];
                uint8_t attr = ch == ' '
                    ? bg
                    : (uint8_t)(bg | DONUT_FG);   /* monochrome */
                o_poke((uint8_t)c2, (uint8_t)r, ch, attr);
            }

        A += 0.04;
        B += 0.02;

        if (kbhit()) {
            uint16_t k = get_key();
            uint8_t ch = (uint8_t)(k & 0xFF);
            if (ch == 0x1B || ch == 'q' || ch == 'Q') break;   /* ESC / q */
        }

        o_delay_ms(20);
    }

    clear_screen();
}
