// PIANO - the OsmiumOS flagship toy.
//
// Two tabs, switched with TAB:
//
//   [1 PLAY]     17-key piano played from the home row. Every note draws a
//                sine wave at the played frequency in the space below the
//                keys. The tone comes straight from the PIT/speaker ports
//                (non-blocking) so the wave animates while the note sounds.
//
//   [2 SEQUENCE] A 16-step x 13-note sequencer built out of CP437 box-drawing
//                characters - the first real UI on OsmiumOS. Move with the
//                arrows, toggle a cell with space, play with `p`.
//
//   play keys: A W S E D F T G Y H U J K O L P ;
//
// Build:  ./build.sh   (from the repo root)   ->  dist/PIANO.BIN

__asm__(".code16gcc\n");
#include "api.h"
#include "ogfx.h"
#include "omath.h"
#include "osnd.h"

#define NKEYS     17
#define CELL      4
#define START_COL 6

#define ROW_KEY   6
#define ROW_NAME  7
#define ROW_LAST  9
#define ROW_FREQ  10
#define ROW_HIST  13
#define HIST_LEN  10

#define NOTE_MS   130
#define FRAMES    8
#define FRAME_MS  16

// Sine display geometry. A 64-wide strip so `x * cycles` lands exactly on
// period boundaries in the 64-entry sine table.
#define WAVE_COL  8
#define WAVE_W    64
#define WAVE_TOP  15
#define WAVE_BOT  23
#define WAVE_H    (WAVE_BOT - WAVE_TOP + 1)
#define WAVE_MID  19
#define WAVE_AMP  4

// ---- sequencer geometry ------------------------------------------------
#define SEQ_STEPS 16
#define SEQ_ROWS  13          /* C4..C5, one octave */
#define BX        5           /* left border column            */
#define PX        6           /* first cell column             */
#define CELLW     4           /* 3 chars + 1 separator         */
#define STEPROW   2
#define TOPROW    3
#define GY        4           /* first note row                */
#define BOTROW    17
#define INFOROW   19
#define HELPROW   20
#define MSGROW    18

// CP437 box drawing
#define B_DA 0xDA
#define B_BF 0xBF
#define B_C0 0xC0
#define B_D9 0xD9
#define B_C4 0xC4
#define B_B3 0xB3
#define B_C2 0xC2
#define B_C1 0xC1
#define B_TRI 0x1F

static const char keys[NKEYS] = {
    'A','W','S','E','D','F','T','G','Y','H','U','J','K','O','L','P',';'
};

static const uint16_t freqs[NKEYS] = {
    262, 277, 294, 311, 330, 349, 370, 392, 415,
    440, 466, 494, 523, 554, 587, 622, 659
};

static const char names[NKEYS][4] = {
    "C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4",
    "A4", "A#4", "B4", "C5", "C#5", "D5", "D#5", "E5"
};

static const uint8_t seqpal[15] = {
    9, 11, 14, 10, 12, 13, 15, 7, 3, 2, 6, 4, 5, 8, 1
};

static char hist[HIST_LEN][4];
static uint8_t base_attr, hi_attr, bgonly;
static uint8_t tabmode;

/* ---- sequencer state --------------------------------------------------- */
static uint8_t grid[SEQ_ROWS][SEQ_STEPS];
static uint8_t sc, sr;              /* cursor step / row        */
static uint8_t bpm = 120;
static uint8_t loop_on = 1;
static uint8_t seq_buf[1 + SEQ_ROWS * SEQ_STEPS];

static void print_at(uint8_t col, uint8_t row, const char* s) {
    gotoxy(col, row);
    print_str(s);
}

static int kbhit(void) {
    uint8_t v;
    __asm__ __volatile__("movb $0x01, %%ah\n\tint $0x16\n\tsetnz %0"
                         : "=q"(v) : : "eax");
    return v;
}

/* ====================================================================== */
/*  PLAY tab                                                              */
/* ====================================================================== */

static void draw_cell(uint8_t i, uint8_t attr) {
    uint8_t c = (uint8_t)(START_COL + i * CELL);
    const char* nm = names[i];

    o_poke((uint8_t)(c + 0), ROW_KEY, ' ',     attr);
    o_poke((uint8_t)(c + 1), ROW_KEY, keys[i], attr);
    o_poke((uint8_t)(c + 2), ROW_KEY, ' ',     attr);
    o_poke((uint8_t)(c + 3), ROW_KEY, ' ',     attr);

    o_poke((uint8_t)(c + 0), ROW_NAME, nm[0], attr);
    o_poke((uint8_t)(c + 1), ROW_NAME, nm[1], attr);
    o_poke((uint8_t)(c + 2), ROW_NAME, nm[2], attr);
    o_poke((uint8_t)(c + 3), ROW_NAME, ' ',   attr);
}

// Number of visible periods for a frequency: 2 (low) .. 10 (high).
static uint8_t wave_cycles(uint16_t freq) {
    return (uint8_t)(2 + ((freq - 262u) * 8u) / 397u);
}

// Draw one frame of the waveform. `phase` (steps of the 64-entry table)
// scrolls the wave so successive frames animate.
static void draw_wave(uint16_t freq, uint8_t phase) {
    uint8_t cycles = wave_cycles(freq);
    uint8_t x;
    int last = WAVE_MID;

    o_fill(WAVE_COL, WAVE_TOP, WAVE_W, WAVE_H, ' ', base_attr);

    for (x = 0; x < WAVE_W; x++) {
        int s = o_sin((uint8_t)(x * cycles + phase));
        int y = WAVE_MID - (s * WAVE_AMP) / 127;
        int lo = y < last ? y : last;
        int hi = y < last ? last : y;
        int yy;

        /* Steep parts of a high note would leave gaps between columns, so
         * connect the previous sample to this one with a vertical run. */
        for (yy = lo; yy <= hi; yy++) {
            if (yy < WAVE_TOP || yy > WAVE_BOT) continue;
            o_poke((uint8_t)(WAVE_COL + x), (uint8_t)yy, O_BLOCK_MED, base_attr);
        }
        last = y;
    }
}

// Sound a note while animating the waveform, then silence the speaker.
static void play_visual(uint16_t freq) {
    uint8_t f;
    o_tone(freq);
    for (f = 0; f < FRAMES; f++) {
        draw_wave(freq, (uint8_t)(f * 8));
        o_delay_ms(FRAME_MS);
    }
    o_tone_off();
}

static void push_hist(const char* nm) {
    uint8_t j, b;
    for (j = 0; j < HIST_LEN - 1; j++)
        for (b = 0; b < 4; b++)
            hist[j][b] = hist[j + 1][b];
    for (b = 0; b < 4; b++)
        hist[HIST_LEN - 1][b] = nm[b];
}

static void draw_hist(void) {
    uint8_t j, b;

    gotoxy(START_COL, ROW_HIST);
    for (b = 0; b < 46; b++) print_char(' ');

    gotoxy(START_COL, ROW_HIST);
    for (j = 0; j < HIST_LEN; j++) {
        for (b = 0; b < 3 && hist[j][b]; b++) print_char(hist[j][b]);
        print_char(' ');
    }
}

static void show_note(uint8_t i) {
    print_at((uint8_t)(START_COL + 11), ROW_LAST, "     ");
    print_at((uint8_t)(START_COL + 11), ROW_LAST, names[i]);
    print_at((uint8_t)(START_COL + 11), ROW_FREQ, "     ");
    gotoxy((uint8_t)(START_COL + 11), ROW_FREQ);
    print_int(freqs[i]);
}

static void draw_tabs(void) {
    o_print_at(29, 0, " 1 PLAY ",     tabmode == 0 ? hi_attr : base_attr);
    o_print_at(38, 0, " 2 SEQUENCE ", tabmode == 1 ? hi_attr : base_attr);
}

static void play_draw(void) {
    int i;
    clear_screen();
    draw_tabs();
    print_at(27, 1, "O S M I U M   P I A N O");
    print_at(20, 2, "--------------------------------------");
    print_at(19, 3, "Press a key to play  |  TAB sequence");
    for (i = 0; i < NKEYS; i++) draw_cell((uint8_t)i, base_attr);
    print_at(START_COL, ROW_LAST, "Last note:");
    print_at(START_COL, ROW_FREQ, "Freq (Hz):");
    print_at(START_COL, ROW_HIST - 1, "Recent:");
    draw_hist();
}

/* Returns 1 to quit the program, 0 to switch to the sequencer tab. */
static int play_run(uint8_t flourish) {
    int i;

    play_draw();

    if (flourish) {
        static const uint8_t fl[4] = { 0, 4, 7, 12 };
        for (i = 0; i < 4; i++) {
            draw_cell(fl[i], hi_attr);
            show_note(fl[i]);
            play_visual(freqs[fl[i]]);
            draw_cell(fl[i], base_attr);
        }
    }

    for (;;) {
        uint16_t k = get_key();
        uint8_t ch = (uint8_t)(k & 0xFF);
        int found = -1;

        if (ch == 0x1B) return 1;                 /* ESC */
        if (ch == 0x09) return 0;                 /* TAB */
        if (ch >= 'a' && ch <= 'z') ch = (uint8_t)(ch - 32);

        for (i = 0; i < NKEYS; i++)
            if ((uint8_t)keys[i] == ch) { found = i; break; }
        if (found < 0) continue;

        draw_cell((uint8_t)found, hi_attr);
        show_note((uint8_t)found);
        play_visual(freqs[found]);
        draw_cell((uint8_t)found, base_attr);

        push_hist(names[found]);
        draw_hist();
    }
}

/* ====================================================================== */
/*  SEQUENCE tab                                                          */
/* ====================================================================== */

static uint8_t pitch_color(uint8_t row) {
    uint8_t note = (uint8_t)(SEQ_ROWS - 1 - row);
    return (uint8_t)(bgonly | seqpal[note % 15]);
}

static void draw_note_cell(uint8_t step, uint8_t row, uint8_t cursor) {
    uint8_t x = (uint8_t)(PX + step * CELLW);
    uint8_t on = grid[row][step];
    uint8_t at;
    char c;

    if (cursor)     at = 0x70;              /* black on white  */
    else if (on)    at = pitch_color(row);
    else            at = base_attr;

    c = on ? (char)O_BLOCK_FULL : ' ';
    o_poke(x,     (uint8_t)(GY + row), c, at);
    o_poke(x + 1, (uint8_t)(GY + row), c, at);
    o_poke(x + 2, (uint8_t)(GY + row), c, at);
}

static void draw_step_numbers(void) {
    for (uint8_t s = 0; s < SEQ_STEPS; s++) {
        uint8_t n = (uint8_t)(s + 1);
        uint8_t c = (uint8_t)(PX + s * CELLW + (n < 10 ? 1 : 0));
        gotoxy(c, STEPROW);
        print_int(n);
    }
}

static void draw_grid_frame(void) {
    /* top border */
    o_poke(BX, TOPROW, B_DA, base_attr);
    for (uint8_t s = 0; s < SEQ_STEPS; s++) {
        uint8_t x = (uint8_t)(PX + s * CELLW);
        o_poke(x,     TOPROW, B_C4, base_attr);
        o_poke(x + 1, TOPROW, B_C4, base_attr);
        o_poke(x + 2, TOPROW, B_C4, base_attr);
        o_poke((uint8_t)(x + 3), TOPROW,
               (uint8_t)(s == SEQ_STEPS - 1 ? B_BF : B_C2), base_attr);
    }

    /* bottom border */
    o_poke(BX, BOTROW, B_C0, base_attr);
    for (uint8_t s = 0; s < SEQ_STEPS; s++) {
        uint8_t x = (uint8_t)(PX + s * CELLW);
        o_poke(x,     BOTROW, B_C4, base_attr);
        o_poke(x + 1, BOTROW, B_C4, base_attr);
        o_poke(x + 2, BOTROW, B_C4, base_attr);
        o_poke((uint8_t)(x + 3), BOTROW,
               (uint8_t)(s == SEQ_STEPS - 1 ? B_D9 : B_C1), base_attr);
    }

    /* vertical bars + note names */
    for (uint8_t r = 0; r < SEQ_ROWS; r++) {
        uint8_t y = (uint8_t)(GY + r);
        o_poke(BX, y, B_B3, base_attr);
        for (uint8_t s = 1; s < SEQ_STEPS; s++)
            o_poke((uint8_t)(BX + s * CELLW), y, B_B3, base_attr);
        o_poke((uint8_t)(BX + SEQ_STEPS * CELLW), y, B_B3, base_attr);

        const char* nm = names[SEQ_ROWS - 1 - r];
        uint8_t len = 0;
        while (nm[len]) len++;
        gotoxy((uint8_t)(BX - 1 - len), y);
        print_str(nm);
    }
}

static void draw_info(void) {
    o_fill(0, INFOROW, 80, 1, ' ', base_attr);
    gotoxy(0, INFOROW);
    print_str("Tempo: ");
    print_int(bpm);
    print_str(" BPM   Loop: ");
    print_str(loop_on ? "on " : "off");
    print_str("   Steps: 16");
}

static void draw_help(void) {
    o_fill(0, HELPROW, 80, 2, ' ', base_attr);
    gotoxy(0, HELPROW);
    print_str("arrows move   space toggle   p play   l loop   c clear");
    gotoxy(0, (uint8_t)(HELPROW + 1));
    print_str("[/] tempo   s save   o load   TAB = piano   ESC quits");
}

static void seq_draw_all(void) {
    clear_screen();
    draw_tabs();
    print_at(29, 1, "O S M I U M   S E Q U E N C E R");
    draw_step_numbers();
    draw_grid_frame();
    for (uint8_t r = 0; r < SEQ_ROWS; r++)
        for (uint8_t s = 0; s < SEQ_STEPS; s++)
            draw_note_cell(s, r, (uint8_t)(s == sc && r == sr));
    draw_info();
    draw_help();
}

static void seq_msg(const char* s) {
    o_fill(0, MSGROW, 80, 1, ' ', base_attr);
    o_print_at(0, MSGROW, s, base_attr);
}

static void seq_mark_playhead(uint8_t step, uint8_t on) {
    o_poke((uint8_t)(PX + step * CELLW + 1), TOPROW,
           on ? (char)B_TRI : (char)B_C4, on ? 0x0F : base_attr);
}

static void seq_play(void) {
    uint16_t num = 60000u;
    uint16_t step_ms = (uint16_t)(num / (uint16_t)(bpm * 4));
    uint8_t step = 0;

    if (step_ms < 20) step_ms = 20;

    for (;;) {
        seq_mark_playhead(step, 1);

        uint8_t n = 0;
        for (uint8_t r = 0; r < SEQ_ROWS; r++) if (grid[r][step]) n++;

        if (n == 0) {
            o_delay_ms(step_ms);
        } else {
            uint16_t each = (uint16_t)(step_ms / n);
            if (each < 10) each = 10;
            for (uint8_t r = 0; r < SEQ_ROWS; r++)
                if (grid[r][step]) play_note(freqs[SEQ_ROWS - 1 - r], each);
        }

        seq_mark_playhead(step, 0);

        if (kbhit()) { get_key(); break; }        /* any key stops */

        step++;
        if (step >= SEQ_STEPS) {
            if (!loop_on) break;
            step = 0;
        }
    }
}

static char seq_name[20];

static int prompt_name(const char* label) {
    int n = 0, lb = 0;
    while (label[lb]) lb++;
    seq_name[0] = '\0';
    for (;;) {
        for (int c = 0; c < 80; c++) o_poke((uint8_t)c, MSGROW, ' ', base_attr);
        o_print_at(0, MSGROW, label, base_attr);
        o_print_at((uint8_t)lb, MSGROW, seq_name, base_attr);
        o_poke((uint8_t)(lb + n), MSGROW, '_', 0x0F);
        uint16_t k = get_key();
        uint8_t ch = (uint8_t)(k & 0xFF);
        if (ch == 0x0D) break;
        if (ch == 0x1B) { n = 0; break; }
        if (ch == 0x08) { if (n > 0) seq_name[--n] = '\0'; continue; }
        if (ch >= 32 && ch < 127 && n < 12) { seq_name[n++] = (char)ch; seq_name[n] = '\0'; }
    }
    seq_name[n] = '\0';
    return n;
}

static void seq_save(void) {
    int n = prompt_name("Save as: ");
    if (n <= 0) { seq_msg("Cancelled."); return; }
    seq_buf[0] = bpm;
    for (uint8_t r = 0; r < SEQ_ROWS; r++)
        for (uint8_t s = 0; s < SEQ_STEPS; s++)
            seq_buf[1 + r * SEQ_STEPS + s] = grid[r][s];
    uint8_t err = fs_write_file(seq_name, seq_buf, (uint16_t)(1 + SEQ_ROWS * SEQ_STEPS));
    seq_msg(err ? "Save failed." : "Saved.");
}

static void seq_load(void) {
    int n = prompt_name("Open: ");
    if (n <= 0) { seq_msg("Cancelled."); return; }
    if (fs_read_file(seq_name, seq_buf, (uint16_t)(1 + SEQ_ROWS * SEQ_STEPS))) {
        seq_msg("Not found.");
        return;
    }
    if (seq_buf[0] >= 40 && seq_buf[0] <= 300) bpm = seq_buf[0];
    for (uint8_t r = 0; r < SEQ_ROWS; r++)
        for (uint8_t s = 0; s < SEQ_STEPS; s++)
            grid[r][s] = seq_buf[1 + r * SEQ_STEPS + s];
    seq_draw_all();
    seq_msg("Loaded.");
}

/* Returns 1 to quit the program, 0 to switch to the piano tab. */
static int seq_run(void) {
    seq_draw_all();

    for (;;) {
        uint16_t k = get_key();
        uint8_t ch = (uint8_t)(k & 0xFF);
        uint8_t scn = (uint8_t)(k >> 8);

        if (ch == 0x1B) return 1;                  /* ESC */
        if (ch == 0x09) return 0;                  /* TAB */

        uint8_t orow = sr, ocol = sc;

        if (scn == 0x48 && ch == 0)      { if (sr > 0) sr--; }
        else if (scn == 0x50 && ch == 0) { if (sr < SEQ_ROWS - 1) sr++; }
        else if (scn == 0x4B && ch == 0) { if (sc > 0) sc--; }
        else if (scn == 0x4D && ch == 0) { if (sc < SEQ_STEPS - 1) sc++; }
        else if (ch == ' ' || ch == 0x0D) {
            grid[sr][sc] = (uint8_t)!grid[sr][sc];
        }
        else if (ch == 'p' || ch == 'P') { seq_play(); draw_info(); }
        else if (ch == 'l' || ch == 'L') { loop_on = (uint8_t)!loop_on; draw_info(); }
        else if (ch == 'c' || ch == 'C') {
            for (uint8_t r = 0; r < SEQ_ROWS; r++)
                for (uint8_t s = 0; s < SEQ_STEPS; s++) grid[r][s] = 0;
            seq_draw_all();
        }
        else if (ch == '[' || ch == '-') { if (bpm > 40) bpm -= 5; draw_info(); }
        else if (ch == ']' || ch == '+' || ch == '=') { if (bpm < 300) bpm += 5; draw_info(); }
        else if (ch == 's' || ch == 'S') { seq_save(); }
        else if (ch == 'o' || ch == 'O') { seq_load(); }
        else continue;

        if (sr != orow || sc != ocol) {
            draw_note_cell(ocol, orow, 0);
            draw_note_cell(sc, sr, 1);
        } else {
            draw_note_cell(sc, sr, 1);
        }
    }
}

/* ====================================================================== */

void module_main(void) {
    base_attr = get_cur_col();
    hi_attr = (uint8_t)((base_attr << 4) | (base_attr >> 4));
    bgonly = (uint8_t)(base_attr & 0xF0);

    for (int i = 0; i < SEQ_ROWS; i++)
        for (int j = 0; j < SEQ_STEPS; j++) grid[i][j] = 0;

    uint8_t flourish = 1;
    tabmode = 0;

    for (;;) {
        int quit;
        if (tabmode == 0) quit = play_run(flourish);
        else              quit = seq_run();
        if (quit) break;
        tabmode = (uint8_t)!tabmode;
        flourish = 0;
    }

    clear_screen();
}
