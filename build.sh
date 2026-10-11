#!/bin/bash
# Build every apps/<name>/<name>.c into a loadable OsmiumOS module,
# dist/<NAME>.BIN. Each app is a single translation unit linked against the
# shared entry stub in sdk/module_entry.asm with sdk/module.ld.
#
# The 22-sector module limit (11,264 bytes) is enforced: a module that grows
# past it is silently truncated by the OS loader, so we refuse to build it.
set -e
cd "$(dirname "$0")"

CC_FLAGS="-m16 -march=i386 -ffreestanding -fno-pic -fno-PIE -fno-stack-protector -nostdlib"
MAX_SECTORS=22

mkdir -p build dist

echo "[*] Assembling module entry stub..."
nasm -f elf32 sdk/module_entry.asm -o build/entry.o

built=0
for dir in apps/*/; do
    [ -d "$dir" ] || continue
    name=$(basename "$dir")
    src="${dir}${name}.c"
    [ -f "$src" ] || continue
    upper=$(printf '%s' "$name" | tr '[:lower:]' '[:upper:]')

    echo "[*] Building $name -> $upper.BIN"
    gcc $CC_FLAGS -Isdk -Ilib -c "$src" -o "build/$name.o"
    ld -m elf_i386 -T sdk/module.ld build/entry.o "build/$name.o" -o "dist/$upper.BIN"

    size=$(stat -c%s "dist/$upper.BIN")
    sectors=$(( (size + 511) / 512 ))
    if [ "$sectors" -gt "$MAX_SECTORS" ]; then
        echo "    !! $upper.BIN is $size bytes ($sectors sectors) - EXCEEDS $MAX_SECTORS"
        exit 1
    fi
    echo "    $upper.BIN  ${size} bytes  (${sectors}/${MAX_SECTORS} sectors)"
    built=$((built + 1))
done

echo "[+] Built ${built} app(s) into dist/"
