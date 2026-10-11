// COWSAY - the classic talking cow.
//
//   cowsay hello there
//   cowsay                     (says the default line)
//
// Build: ./build.sh -> dist/COWSAY.BIN

__asm__(".code16gcc\n");
#include "api.h"

#define MAX_LINES 7
#define MAX_W     38

static char lines[MAX_LINES][MAX_W + 1];
static int  nlines;

static const char* cow[] = {
    "        \\   ^__^",
    "         \\  (oo)\\_______",
    "            (__)\\       )\\/\\",
    "                ||----w |",
    "                ||     ||"
};
#define COW_LINES 5

static void wrap_words(const char* msg) {
    int li = 0, col = 0;
    const char* p = msg;

    lines[0][0] = '\0';
    while (*p && li < MAX_LINES) {
        while (*p == ' ') p++;
        if (!*p) break;

        const char* s = p;
        int wl = 0;
        while (p[wl] && p[wl] != ' ') wl++;

        if (col > 0 && col + 1 + wl > MAX_W) { li++; col = 0; if (li >= MAX_LINES) break; }
        if (col > 0) lines[li][col++] = ' ';

        for (int i = 0; i < wl && col < MAX_W; i++) lines[li][col++] = s[i];
        lines[li][col] = '\0';

        p += wl;
    }
    nlines = li + 1;
}

static void repeat(char c, int n) {
    while (n-- > 0) print_char(c);
}

void module_main(void) {
    const char* msg = (const char*)MODULE_ARGS;
    if (!msg || !msg[0]) msg = "Moo. I am a cow. moo.";

    wrap_words(msg);

    int maxw = 0;
    for (int i = 0; i < nlines; i++) {
        int n = 0;
        while (lines[i][n]) n++;
        if (n > maxw) maxw = n;
    }

    clear_screen();

    print_char(' ');
    repeat('_', maxw + 2);
    print_str("\r\n");

    for (int i = 0; i < nlines; i++) {
        char left, right;
        if (nlines == 1)            { left = '<'; right = '>'; }
        else if (i == 0)            { left = '/'; right = '\\'; }
        else if (i == nlines - 1)   { left = '\\'; right = '/'; }
        else                        { left = '|'; right = '|'; }

        print_char(left);
        print_char(' ');
        print_str(lines[i]);
        int n = 0;
        while (lines[i][n]) n++;
        repeat(' ', maxw - n);
        print_char(' ');
        print_char(right);
        print_str("\r\n");
    }

    print_char(' ');
    repeat('-', maxw + 2);
    print_str("\r\n");

    for (int i = 0; i < COW_LINES; i++) {
        print_str(cow[i]);
        print_str("\r\n");
    }
}
