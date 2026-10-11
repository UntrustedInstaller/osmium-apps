#!/bin/bash
# Refresh the vendored SDK files from a local OsmiumOS checkout.
# Override the checkout location with OSMIUM_DIR, e.g.
#   OSMIUM_DIR=~/src/osmium ./sync-sdk.sh
set -e
cd "$(dirname "$0")"

OSMIUM="${OSMIUM_DIR:-../osmium}"

if [ ! -f "$OSMIUM/src/app/api.h" ]; then
    echo "ERR: no OsmiumOS checkout at '$OSMIUM' (set OSMIUM_DIR)" >&2
    exit 1
fi

cp "$OSMIUM/src/app/api.h"           sdk/api.h
cp "$OSMIUM/src/module_entry.asm"    sdk/module_entry.asm
cp "$OSMIUM/src/module.ld"           sdk/module.ld
echo "[+] SDK refreshed from $OSMIUM"
