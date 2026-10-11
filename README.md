# OsmiumOS Apps

Optional, loadable applications for [OsmiumOS](https://github.com/UntrustedInstaller/osmium).
Each app is a single `.BIN` module that the kernel loads at `0x2000` and enters
via `lcall`, talking to the OS exclusively through the `INT 60h` API.

Keeping apps out of the core repo keeps the kernel lean (the kernel image has a
hard 78-sector budget) and lets them be dropped onto a floppy image or swapped
in and out freely.

## Layout

```
sdk/      vendored build contract (api.h, module_entry.asm, module.ld)
lib/      shared inline helpers (VGA pokes, sine table, tone I/O, strings)
apps/     one directory per app:  apps/<name>/<name>.c  ->  dist/<NAME>.BIN
build.sh  builds every app in one go
dist/     build output (gitignored)
```

## Quick start

Dependencies: `gcc` (with `-m16` support), `nasm`, `ld`. To run them you also
need `mtools` and `qemu-system-i386`.

```bash
./build.sh                       # -> dist/PIANO.BIN, dist/...
mcopy -i /path/to/os.img dist/PIANO.BIN ::PIANO.BIN
```

Boot the image and type the command name (`piano`, `donut`, ...). Filenames are
8.3 and the filename *is* the command, so keep app names ≤ 8 characters.

## The module contract

Everything here is just the kernel's documented module ABI (see the main repo's
`docs/MODULE.md`). The gotchas worth repeating:

- **Entry:** `void module_main(void);` and `.code16gcc` must be the first line
  of the translation unit (`__asm__(".code16gcc\n");`).
- **Segments:** the loader runs you with `DS = ES = SS = 0x2000`, `SP = 0xFE00`.
- **Stack:** only ~512 bytes before you clobber the argument block at `0xFC00`.
  Keep local buffers small.
- **Args:** the command tail arrives as a NUL-terminated string at
  `MODULE_ARGS` (`0x2000:0xFC00`). Each app parses its own switches.
- **Size:** 22 sectors / **11,264 bytes** max. `build.sh` refuses to build
  anything larger, because the loader truncates silently.
- **No libc, no libgcc:** `ld` is invoked directly, so 32-bit division/multiply
  helpers are unavailable. `int` is 16-bit. `lib/osnd.h` shows the usual escape
  hatch (a 16-bit `div` in inline asm). Use `long` (32-bit) for products of two
  16-bit values, and avoid `long * long`.
- **No optimization** is used, matching the kernel's flags; keep helper
  functions `static inline` so unused ones cost nothing.

## Apps

| Command | What it is |
|---------|------------|
| `piano` | 17-key piano with an animated sine-wave display |
| `donut` | rotating 3D ASCII donut (a `donut.c` port) |
| `sort`  | sorting-algorithm visualizer |
| `fractal` | Mandelbrot / Julia set renderer |
| `cowsay` | the classic talking cow |
| `paint` | ASCII paint program (saves to disk) |
| *(sequencer)* | a second tab inside `piano` (step sequencer) |

## Adding an app

1. `mkdir apps/mytool && $EDITOR apps/mytool/mytool.c`
2. Write `void module_main(void)` that includes `"api.h"` plus any `lib/` header.
3. `./build.sh` → `dist/MYTOOL.BIN`
4. `mcopy -i .../os.img dist/MYTOOL.BIN ::MYTOOL.BIN`

## License

GPLv3, same as OsmiumOS. See `LICENSE`.
