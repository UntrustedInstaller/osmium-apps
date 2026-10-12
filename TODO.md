# TODO / Roadmap

## Planned apps

### Screensavers (classic ASCII / terminal demos)
- [x] `MATRIX.BIN` — falling-glyph rain (`cmatrix`-style), black background
- [x] `STARS.BIN`  — 3D starfield / warp tunnel
- [x] `FIRE.BIN`   — `aafire`-style flickering flame
- [ ] `PIPES.BIN`  — the Windows "3D Pipes" screensaver in ASCII: pipes grow, turn, and colour-cycle
- [ ] `AQUA.BIN`   — asciiquarium-style fish tank (may be tight against the 22-sector cap)

`DONUT.BIN` was the original. Matrix, Stars and Fire are done; Pipes and Aqua
remain.

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
