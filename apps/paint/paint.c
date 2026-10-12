// PAINT - an 80x24 ASCII paint program that saves to disk.
//
// The canvas is a uint16_t[80*24] in exactly VGA text layout
// (low byte = character, high byte = attribute), so blitting is a single
// `rep movsw` and save/load is a raw 3840-byte dump.
//
//   arrows          move cursor
//   prnt chars      paint         backspace  erase left
//   Ctrl+F          next colour   Ctrl+B     next background
//   Ctrl+N          new canvas    Ctrl+S     save
//   Ctrl+O          open          ESC/Ctrl+Q quit
//
// Build: ./build.sh -> dist/PAINT.BIN

__asm__(".code16gcc\n");
#include "api.h"
#include "ogfx.h"

#define CW 80
#define CH 24

static uint16_t canvas[CW * CH];
static char     fname[20];

static uint8_t cx, cy, fg, bg;
static const char* note;

#define ATTR ((uint8_t)((bg << 4) | fg))
#define CELL (canvas[cy * CW + cx])

static void blit(void) {
    uint16_t src = (uint16_t)(uint32_t)canvas;
    __asm__ __volatile__(
        "cld\n\t"
        "pushw %%es\n\t"
        "movw $0xB800, %%ax\n\t"
        "movw %%ax, %%es\n\t"
        "xorw %%di, %%di\n\t"
        "rep movsw\n\t"
        "popw %%es\n\t"
        : : "S"(src), "c"((uint16_t)(CW * CH)) : "ax", "di", "memory"
    );
}

static void draw_cursor(void) {
    uint16_t cell = CELL;
    uint8_t ch = (uint8_t)(cell & 0xFF);
    uint8_t at = (uint8_t)(cell >> 8);
    uint8_t inv = (uint8_t)(((at & 0x0F) << 4) | ((at >> 4) & 0x0F));
    o_poke(cx, cy, ch, inv);
}

static void blit_cursor(void) {
    blit();
    draw_cursor();
}

static void toolbar(void) {
    const char* s = "arrows move  type  CTRL+F/B colour  CTRL+N new  CTRL+S/O  CTRL+Q";
    for (uint8_t i = 0; i < 80; i++) o_poke(i, 24, ' ', 0x70);
    o_print_at(0, 24, s, 0x70);
    /* foreground swatch */
    o_print_at(65, 24, "FG", 0x70);
    o_poke(68, 24, O_BLOCK_FULL, (uint8_t)((bg << 4) | fg));
    o_poke(69, 24, O_BLOCK_FULL, (uint8_t)((bg << 4) | fg));
    /* background swatch */
    o_print_at(71, 24, "BG", 0x70);
    o_poke(74, 24, ' ', (uint8_t)((bg << 4) | 7));
    o_poke(75, 24, ' ', (uint8_t)((bg << 4) | 7));
}

static void message(const char* s) {
    uint8_t c = 0;
    while (s[c]) { o_poke(c, 24, s[c], 0x0F); c++; }
    while (c < 80) { o_poke(c, 24, ' ', 0x0F); c++; }
}

static void new_canvas(void) {
    for (int i = 0; i < CW * CH; i++)
        canvas[i] = (uint16_t)(((uint16_t)ATTR << 8) | (uint16_t)' ');
}

/* Read a filename on the status line. Returns length, or 0 if cancelled. */
static int prompt_name(const char* label) {
    int n = 0, lb = 0;
    while (label[lb]) lb++;
    fname[0] = '\0';
    for (;;) {
        for (int c = 0; c < 80; c++) o_poke((uint8_t)c, 24, ' ', 0x0F);
        o_print_at(0, 24, label, 0x0F);
        o_print_at((uint8_t)lb, 24, fname, 0x0F);
        o_poke((uint8_t)(lb + n), 24, '_', 0x0F);
        uint16_t k = get_key();
        uint8_t ch = (uint8_t)(k & 0xFF);
        if (ch == 0x0D) break;
        if (ch == 0x1B) { n = 0; break; }
        if (ch == 0x08) { if (n > 0) fname[--n] = '\0'; continue; }
        if (ch >= 32 && ch < 127 && n < 12) { fname[n++] = (char)ch; fname[n] = '\0'; }
    }
    fname[n] = '\0';
    return n;
}

void module_main(void) {
    cx = cy = 0;
    fg = 7;
    bg = 0;
    fname[0] = '\0';

    new_canvas();
    blit_cursor();
    toolbar();

    for (;;) {
        uint16_t k = get_key();
        uint8_t ch = (uint8_t)(k & 0xFF);
        uint8_t sc = (uint8_t)(k >> 8);
        int redraw = 1;

        note = 0;

        if (ch == 0x1B || ch == 0x11) break;                 /* ESC / Ctrl+Q */

        if (sc == 0x48 && ch == 0) { if (cy > 0) cy--; }           /* up    */
        else if (sc == 0x50 && ch == 0) { if (cy < CH - 1) cy++; } /* down  */
        else if (sc == 0x4B && ch == 0) { if (cx > 0) cx--; }      /* left  */
        else if (sc == 0x4D && ch == 0) { if (cx < CW - 1) cx++; } /* right */
        else if (sc == 0x53 && ch == 0) { CELL = (uint16_t)(((uint16_t)ATTR << 8) | ' '); }
        else if (ch == 0x08) { if (cx > 0) { cx--; CELL = (uint16_t)(((uint16_t)ATTR << 8) | ' '); } }
        else if (ch == 0x0D) { cx = 0; cy = (uint8_t)((cy + 1) % CH); }
        else if (ch == 0x09) { cx = (uint8_t)(((cx / 8) + 1) * 8); if (cx > CW - 1) cx = 0; }
        else if (ch == 0x06) { fg = (uint8_t)((fg + 1) & 15); }    /* Ctrl+F */
        else if (ch == 0x02) { bg = (uint8_t)((bg + 1) & 15); }    /* Ctrl+B */
        else if (ch == 0x0E) { new_canvas(); }                     /* Ctrl+N */
        else if (ch == 0x13) {                                     /* Ctrl+S */
            int n = prompt_name("Save as: ");
            if (n > 0) note = fs_write_file(fname, canvas, CW * CH * 2) ? "Save failed." : "Saved.";
            else       note = "Cancelled.";
        }
        else if (ch == 0x0F) {                                     /* Ctrl+O */
            int n = prompt_name("Open: ");
            if (n > 0) {
                if (!fs_read_file(fname, canvas, CW * CH * 2)) note = "Loaded.";
                else { new_canvas(); note = "Not found."; }
            } else note = "Cancelled.";
        }
        else if (ch >= 32 && ch < 127) {
            CELL = (uint16_t)(((uint16_t)ATTR << 8) | ch);
            if (cx < CW - 1) cx++;
        }
        else redraw = 0;

        if (redraw) blit_cursor();
        if (note) message(note); else toolbar();
    }

    clear_screen();
}
