# TODO / Roadmap

## Planned apps

### Screensavers (classic ASCII / terminal demos)
- [ ] `MATRIX.BIN` — falling green katakana rain (`cmatrix`-style) on a forced black background
- [ ] `PIPES.BIN`  — the Windows "3D Pipes" screensaver in ASCII: pipes grow, turn, and colour-cycle
- [ ] `STARS.BIN`  — starfield / warp tunnel
- [ ] `AQUA.BIN`   — asciiquarium-style fish tank (may be tight against the 22-sector cap)
- [ ] `FIRE.BIN`   — `aafire`-style flickering flame

`DONUT.BIN` already covers the classic donut; these are the other demos usually
shipped alongside it. Matrix and Pipes are the priority pair.

## Theme / colour handling

- [x] Apps read the selected theme from `CONFIG.BIN` via `lib/otheme.h`
      (falls back to the live kernel colour if the file is missing).
- [x] Donut and Fractal force a black background (like Paint).
- [x] Fixed the core theme-4 mismatch: `load_theme` now uses `0x07` (grey),
      matching `cmd_theme`.

## Core follow-ups (from the main repo)

- [ ] `ls <path>` — currently always lists the current directory
- [ ] Lift `cat`'s 512-byte cap
- [ ] Fix the stale `INT 60h` comment near `src/kernel.asm:426`
