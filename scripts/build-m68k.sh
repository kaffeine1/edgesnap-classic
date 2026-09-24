#!/bin/sh
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Build the classic lane with the bebbo toolchain in ~/amiga-dev/toolchain
# and check that what came out are Amiga executables.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export PATH="$HOME/amiga-dev/toolchain/bin:$PATH"
cd "$ROOT"
make -f Makefile.m68k "$@"
for f in build/m68k/edgesnap.library build/m68k/EdgeSnap \
         build/m68k/EdgeSnap-static build/m68k/EdgeSnapPrefs \
         build/m68k/esnaptest; do
    file "$f" | grep -q "AmigaOS loadseg" || {
        echo "ERROR: $f is not an AmigaOS executable" >&2
        exit 1
    }
done
echo "classic lane: build/m68k/{edgesnap.library,EdgeSnap,EdgeSnap-static,EdgeSnapPrefs,esnaptest}"
