// DONUT - rotating 3D ASCII torus, after Andy Sloane's donut.c.
//
// The original uses libm sin/cos; here they come straight from the x87 FPU
// (fsin/fcos) so the module links with no libc. ESC quits.
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

static int kbhit(void) {
    uint8_t v;
    __asm__ __volatile__("movb $0x01, %%ah\n\tint $0x16\n\tsetnz %0"
                         : "=q"(v) : : "eax");
    return v;
}

static int ramp_index(char ch) {
    for (int i = 0; i < 12; i++)
        if (ramp[i] == ch) return i;
    return 0;
}

void module_main(void) {
    double A = 0, B = 0;
    uint8_t bg = (uint8_t)(get_cur_col() & 0xF0);

    clear_screen();

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
                    : (uint8_t)(bg | (uint8_t)(1 + ramp_index(ch) % 14));
                o_poke((uint8_t)c2, (uint8_t)r, ch, attr);
            }

        A += 0.04;
        B += 0.02;

        if (kbhit() && get_key() == 0x011B) break;   /* ESC */

        o_delay_ms(20);
    }

    clear_screen();
}
