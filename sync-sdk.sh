#!/bin/bash
# Refresh the vendored SDK files (sdk/api.h, sdk/module_entry.asm, sdk/module.ld)
# from OsmiumOS.
#
# Default: shallow-clone the public repo at OSMIUM_REF (a branch or tag).
#   OSMIUM_REF=v1.0.0 ./sync-sdk.sh
#
# To sync from a local working checkout instead (useful while hacking on the
# kernel before pushing), point OSMIUM_DIR at it:
#   OSMIUM_DIR=~/src/osmium ./sync-sdk.sh
set -euo pipefail
cd "$(dirname "$0")"

REPO="${OSMIUM_REPO:-https://github.com/UntrustedInstaller/osmium.git}"
REF="${OSMIUM_REF:-main}"
SRC="${OSMIUM_DIR:-}"

tmp=""
cleanup() {
    if [ -n "$tmp" ]; then rm -rf "$tmp"; fi
    return 0
}
trap cleanup EXIT

if [ -n "$SRC" ]; then
    [ -f "$SRC/src/app/api.h" ] || {
        echo "ERR: no OsmiumOS checkout at '$SRC' (unset OSMIUM_DIR to fetch from $REPO)" >&2
        exit 1
    }
else
    command -v git >/dev/null 2>&1 || { echo "ERR: git is required to fetch $REPO" >&2; exit 1; }
    tmp="$(mktemp -d)"
    echo "[*] Fetching $REPO ($REF) ..."
    if ! git clone --quiet --depth 1 --branch "$REF" "$REPO" "$tmp"; then
        echo "ERR: could not fetch ref '$REF' from $REPO" >&2
        exit 1
    fi
    SRC="$tmp"
fi

cp "$SRC/src/app/api.h"        sdk/api.h
cp "$SRC/src/module_entry.asm" sdk/module_entry.asm
cp "$SRC/src/module.ld"        sdk/module.ld
echo "[+] SDK refreshed from $SRC"

# Report whether anything actually changed so the caller knows to commit.
if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    if git diff --quiet -- sdk; then
        echo "[=] sdk/ already up to date (no changes)"
    else
        echo "[!] sdk/ changed - review and commit"
    fi
fi
