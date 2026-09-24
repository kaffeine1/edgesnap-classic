#!/bin/sh
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Build the classic lane with the bebbo toolchain in ~/amiga-dev/toolchain
# and check that what came out is an Amiga executable.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export PATH="$HOME/amiga-dev/toolchain/bin:$PATH"
cd "$ROOT"
make -f Makefile.m68k "$@"
file build/m68k/EdgeSnap | grep -q "AmigaOS loadseg" || {
    echo "ERROR: build/m68k/EdgeSnap is not an AmigaOS executable" >&2
    exit 1
}
echo "classic lane: build/m68k/EdgeSnap"
