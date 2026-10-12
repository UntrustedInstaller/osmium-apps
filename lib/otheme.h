// otheme.h - read the user's selected theme from CONFIG.BIN.
//
// The kernel maps a theme index (0..4) to a text attribute and saves that index
// to CONFIG.BIN when the user runs `theme`. Apps use this to match the shell
// instead of assuming a background colour.
//
// Keep o_theme_table in sync with the kernel's tables (src/main.c load_theme
// and src/app/theme.c cmd_theme).

#ifndef OTHEME_H
#define OTHEME_H

#include "api.h"

#define O_THEME_FILE "\\CONFIG.BIN"   /* absolute: works from any cwd */

static const uint8_t o_theme_table[5] = {0x1F, 0x02, 0x06, 0x04, 0x07};

/* Attribute of the saved theme, or of the live kernel colour if CONFIG.BIN
 * cannot be read (e.g. it was deleted). */
static inline uint8_t o_theme_attr(void) {
    uint8_t cfg[2];
    cfg[0] = 0xFF;
    cfg[1] = 0;
    if (fs_read_file(O_THEME_FILE, cfg, sizeof(cfg)) == 0 && cfg[0] <= 4)
        return o_theme_table[cfg[0]];
    return get_cur_col();
}

/* Background nibble of the saved theme (ready to OR with a foreground). */
static inline uint8_t o_theme_bg(void) {
    return (uint8_t)(o_theme_attr() & 0xF0);
}

#endif
