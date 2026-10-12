// SORT - sorting-algorithm visualizer.
//
// 76 bars race through nine classic sorts, with an optional beep per write.
//
//   1-9  pick algorithm      space  run        s  shuffle
//   m    toggle sound        any key  stop     q/ESC  quit
//
// Build: ./build.sh -> dist/SORT.BIN

__asm__(".code16gcc\n");
#include "api.h"
#include "ogfx.h"
#include "osnd.h"

#define N    76
#define X0   2
#define Y0   1
#define ROWS 23
#define NADDR 9

static uint8_t val[N];
static uint8_t sound_on = 1;
static uint8_t algo = 0;
static uint16_t n_cmp, n_set;
static uint16_t rng_state;

static const char* algo_names[NADDR] = {
    "bubble   ", "insertion", "selection", "shell    ", "cocktail ",
    "gnome    ", "quick    ", "merge    ", "heap     "
};

static const uint8_t palette[15] = {
    9, 11, 14, 10, 12, 13, 15, 7, 3, 2, 6, 4, 5, 8, 1
};

static uint8_t bg;
static uint8_t aborting;

static uint16_t rng(void) {
    rng_state ^= rng_state << 7;
    rng_state ^= rng_state >> 9;
    rng_state ^= rng_state << 8;
    return rng_state;
}

static void seed_rng(void) {
    uint16_t cx, dx;
    __asm__ __volatile__("movb $0x00, %%ah\n\tint $0x1a"
                         : "=c"(cx), "=d"(dx) : : "eax");
    rng_state = (uint16_t)(dx ^ 0xACE1u);
    if (!rng_state) rng_state = 0xBEEF;
}

static uint8_t color_for(uint8_t h) {
    return palette[(uint8_t)(((uint16_t)h * 14u) / ROWS)];
}

static void draw_col(uint8_t i) {
    uint8_t h = val[i];
    uint8_t attr = color_for(h);
    for (uint8_t r = 0; r < ROWS; r++)
        o_poke((uint8_t)(X0 + i), (uint8_t)(Y0 + r),
               r < h ? O_BLOCK_FULL : ' ', r < h ? attr : bg);
}

static void draw_all(void) {
    for (uint8_t i = 0; i < N; i++) draw_col(i);
}

static void hud(void) {
    o_fill(0, 0, 80, 1, ' ', bg);
    gotoxy(1, 0);
    print_str("Sort: ");
    print_str(algo_names[algo]);
    print_str("  Compares: ");
    print_int(n_cmp);
    print_str("  Writes: ");
    print_int(n_set);
    print_str("  Sound: ");
    print_str(sound_on ? "on " : "off");
}

static void controls(void) {
    o_fill(0, 24, 80, 1, ' ', bg);
    gotoxy(1, 24);
    print_str("1-9 algorithm   space run   s shuffle   m sound   any key stops   q quit");
}

static int kbhit(void) {
    uint8_t v;
    __asm__ __volatile__ ("movb $0x01, %%ah\n\tint $0x16\n\tsetnz %0" : "=q"(v) : : "eax");
    return v;
}

/* A run can feel like a hang on real hardware, so let any keypress stop it.
 * Checked on every compare and every write. */
static void poll_abort(void) {
    if (aborting) return;
    if (kbhit()) { (void)get_key(); aborting = 1; }
}

static void set_val(uint8_t i, uint8_t v) {
    if (aborting) return;
    poll_abort();
    if (aborting) return;
    val[i] = v;
    draw_col(i);
    n_set++;
    hud();
    if (sound_on) {
        o_tone((uint16_t)(140 + (uint16_t)v * 34));
        o_delay_ms(3);
        o_tone_off();
    } else {
        o_delay_ms(1);
    }
}

static uint8_t greater(uint8_t a, uint8_t b) {
    n_cmp++;
    poll_abort();
    return a > b;
}

static void swap_ij(uint8_t i, uint8_t j) {
    uint8_t t = val[i];
    set_val(i, val[j]);
    set_val(j, t);
}

/* ---- the algorithms ------------------------------------------------ */

static void s_bubble(void) {
    for (uint8_t i = 0; i < N - 1; i++) {
        if (aborting) return;
        for (uint8_t j = 0; j < N - 1 - i; j++) {
            if (aborting) return;
            if (greater(val[j], val[j + 1])) swap_ij((uint8_t)j, (uint8_t)(j + 1));
        }
    }
}

static void s_insertion(void) {
    for (uint8_t i = 1; i < N; i++) {
        uint8_t key = val[i];
        int j = i - 1;
        if (aborting) return;
        while (j >= 0 && greater(val[j], key)) {
            set_val((uint8_t)j + 1, val[j]);
            j--;
            if (aborting) return;
        }
        if ((uint8_t)(j + 1) != i) set_val((uint8_t)(j + 1), key);
    }
}

static void s_selection(void) {
    for (uint8_t i = 0; i < N - 1; i++) {
        uint8_t m = i;
        if (aborting) return;
        for (uint8_t j = i + 1; j < N; j++) {
            if (aborting) return;
            if (greater(val[m], val[j])) m = j;
        }
        if (m != i) swap_ij(i, m);
    }
}

static void s_shell(void) {
    for (int gap = N / 2; gap > 0; gap /= 2)
        for (int i = gap; i < N; i++) {
            uint8_t tmp = val[i];
            int j = i;
            if (aborting) return;
            while (j >= gap && greater(val[j - gap], tmp)) {
                set_val((uint8_t)j, val[j - gap]);
                j -= gap;
                if (aborting) return;
            }
            if (j != i) set_val((uint8_t)j, tmp);
        }
}

static void s_cocktail(void) {
    int lo = 0, hi = N - 1;
    while (lo < hi) {
        if (aborting) return;
        for (int j = lo; j < hi; j++) {
            if (aborting) return;
            if (greater(val[j], val[j + 1])) swap_ij((uint8_t)j, (uint8_t)(j + 1));
        }
        hi--;
        for (int j = hi; j > lo; j--) {
            if (aborting) return;
            if (greater(val[j - 1], val[j])) swap_ij((uint8_t)(j - 1), (uint8_t)j);
        }
        lo++;
    }
}

static void s_gnome(void) {
    int i = 0;
    while (i < N) {
        if (aborting) return;
        if (i == 0 || !greater(val[i - 1], val[i])) i++;
        else { swap_ij((uint8_t)(i - 1), (uint8_t)i); i--; }
    }
}

static void s_quick(void) {
    uint16_t slo[24], shi[24];
    int sp = 0;
    slo[0] = 0; shi[0] = N - 1; sp = 1;

    while (sp > 0) {
        if (aborting) return;
        sp--;
        int lo = slo[sp], hi = shi[sp];
        if (lo >= hi) continue;

        uint8_t pivot = val[hi];
        int i = lo;
        for (int j = lo; j < hi; j++) {
            if (aborting) return;
            if (!greater(val[j], pivot)) { swap_ij((uint8_t)i, (uint8_t)j); i++; }
        }
        swap_ij((uint8_t)i, (uint8_t)hi);

        if (i - 1 > lo && sp < 23) { slo[sp] = lo; shi[sp] = i - 1; sp++; }
        if (i + 1 < hi && sp < 23) { slo[sp] = i + 1; shi[sp] = hi; sp++; }
    }
}

static void s_merge(void) {
    static uint8_t tmp[N];
    for (int width = 1; width < N; width *= 2) {
        for (int lo = 0; lo < N; lo += 2 * width) {
            int mid = lo + width; if (mid > N) mid = N;
            int hi = lo + 2 * width; if (hi > N) hi = N;
            int i = lo, j = mid, t = 0;
            if (aborting) return;
            while (i < mid && j < hi) {
                if (aborting) return;
                if (!greater(val[j], val[i])) tmp[t++] = val[i++];
                else tmp[t++] = val[j++];
            }
            while (i < mid) tmp[t++] = val[i++];
            while (j < hi) tmp[t++] = val[j++];
            for (int k = 0; k < t; k++) set_val((uint8_t)(lo + k), tmp[k]);
        }
    }
}

static void sift(int root, int end) {
    while (root * 2 + 1 <= end) {
        int child = root * 2 + 1;
        if (aborting) return;
        if (child + 1 <= end && greater(val[child], val[child + 1])) child++;
        if (greater(val[root], val[child])) { swap_ij((uint8_t)root, (uint8_t)child); root = child; }
        else return;
    }
}

static void s_heap(void) {
    for (int start = (N - 2) / 2; start >= 0; start--) {
        if (aborting) return;
        sift(start, N - 1);
    }
    for (int end = N - 1; end > 0; end--) {
        if (aborting) return;
        swap_ij(0, (uint8_t)end);
        sift(0, end - 1);
    }
}

static void run_algo(void) {
    aborting = 0;
    switch (algo) {
        case 0: s_bubble();    break;
        case 1: s_insertion(); break;
        case 2: s_selection(); break;
        case 3: s_shell();     break;
        case 4: s_cocktail();  break;
        case 5: s_gnome();     break;
        case 6: s_quick();     break;
        case 7: s_merge();     break;
        default: s_heap();     break;
    }
    if (!aborting) {
        play_note(660, 60); play_note(880, 60); play_note(1320, 90);
    }
}

/* ---- setup --------------------------------------------------------- */

static void shuffle(void) {
    for (int i = 0; i < N; i++) val[i] = (uint8_t)(1 + (i * ROWS) / N);
    for (int i = N - 1; i > 0; i--) {
        int j = (int)(rng() % (uint16_t)(i + 1));
        uint8_t t = val[i]; val[i] = val[j]; val[j] = t;
    }
    n_cmp = 0; n_set = 0;
}

void module_main(void) {
    bg = (uint8_t)(get_cur_col() & 0xF0);
    seed_rng();
    clear_screen();
    shuffle();
    draw_all();
    hud();
    controls();

    for (;;) {
        uint16_t k = get_key();
        uint8_t ch = (uint8_t)(k & 0xFF);

        if (ch == 0x1B || ch == 'q' || ch == 'Q') break;

        if (ch >= '1' && ch <= '9') {
            algo = (uint8_t)(ch - '1');
            shuffle();
            draw_all();
            hud();
        } else if (ch == 's' || ch == 'S' || ch == 'r' || ch == 'R') {
            shuffle();
            draw_all();
            hud();
        } else if (ch == 'm' || ch == 'M') {
            sound_on = (uint8_t)!sound_on;
            hud();
        } else if (ch == ' ') {
            run_algo();
            hud();
        }
    }

    clear_screen();
}
