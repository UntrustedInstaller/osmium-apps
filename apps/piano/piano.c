// PIANO - the OsmiumOS flagship toy.
//
// A 17-key piano played from the keyboard home row. Every note draws a sine
// wave at the played frequency in the empty space below the keys. The tone is
// driven directly from the PIT/speaker ports (non-blocking) so the waveform
// can animate while the note sounds.
//
//   keys:  A W S E D F T G Y H U J K O L P ;
//   quit:  ESC
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

static char hist[HIST_LEN][4];
static uint8_t base_attr;

static void print_at(uint8_t col, uint8_t row, const char* s) {
    gotoxy(col, row);
    print_str(s);
}

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

    o_fill(WAVE_COL, WAVE_TOP, WAVE_W, WAVE_H, ' ', base_attr);

    for (x = 0; x < WAVE_W; x++) {
        int s = o_sin((uint8_t)(x * cycles + phase));
        int y = WAVE_MID - (s * WAVE_AMP) / 127;
        o_poke((uint8_t)(WAVE_COL + x), (uint8_t)y, O_BLOCK_MED, base_attr);
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

void module_main(void) {
    int i;
    uint8_t hi_attr;

    base_attr = get_cur_col();
    hi_attr = (uint8_t)((base_attr << 4) | (base_attr >> 4));

    clear_screen();

    print_at(27, 1, "O S M I U M   P I A N O");
    print_at(20, 2, "--------------------------------------");
    print_at(19, 3, "Press a key to play  |  ESC to quit");

    for (i = 0; i < NKEYS; i++) draw_cell((uint8_t)i, base_attr);

    print_at(START_COL, ROW_LAST, "Last note:");
    print_at(START_COL, ROW_FREQ, "Freq (Hz):");
    print_at(START_COL, ROW_HIST - 1, "Recent:");

    /* Startup flourish: C4 E4 G4 C5 */
    {
        static const uint8_t flourish[4] = { 0, 4, 7, 12 };
        for (i = 0; i < 4; i++) {
            draw_cell(flourish[i], hi_attr);
            show_note(flourish[i]);
            play_visual(freqs[flourish[i]]);
            draw_cell(flourish[i], base_attr);
        }
    }

    for (;;) {
        uint16_t k = get_key();
        uint8_t ch = (uint8_t)(k & 0xFF);
        int found = -1;

        if (ch == 0x1B) break;                            /* ESC */
        if (ch >= 'a' && ch <= 'z') ch = (uint8_t)(ch - 32);

        for (i = 0; i < NKEYS; i++) {
            if ((uint8_t)keys[i] == ch) { found = i; break; }
        }
        if (found < 0) continue;

        draw_cell((uint8_t)found, hi_attr);
        show_note((uint8_t)found);
        play_visual(freqs[found]);
        draw_cell((uint8_t)found, base_attr);

        push_hist(names[found]);
        draw_hist();
    }

    clear_screen();
}
