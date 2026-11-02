#!/bin/sh
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Build the LhA archive of the classic package, the one for AmigaOS 3.1
# and later on 68k, the way scripts/make-release.sh builds the other
# systems': the same checks, one lane.
#
#   scripts/make-release-68k.sh [version]
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
VERSION="${1:-0.51}"

HDR="$ROOT/include/edgesnap_version.h"
HDR_VERSION=$(sed -n 's/^#define ES_VERSION  *"\([^"]*\)".*/\1/p' "$HDR")
if [ "$HDR_VERSION" != "$VERSION" ]; then
    echo "ERROR: building $VERSION but edgesnap_version.h says $HDR_VERSION" >&2
    exit 1
fi
for pair in \
    "installer/Install:(set #app-version \"$VERSION beta\")" \
    "packaging/EdgeSnap.guide.in:EdgeSnap $VERSION beta" \
    "scripts/stage-package-68k.sh:Version:      $VERSION (beta)"
do
    f="${pair%%:*}"
    want="${pair#*:}"
    if ! grep -qF "$want" "$ROOT/$f"; then
        echo "ERROR: $f does not carry version $VERSION" >&2
        exit 1
    fi
done
echo "version $VERSION agrees across header, installer, guide and staging"

if ! make -s -f "$ROOT/Makefile.host" -C "$ROOT" test >/dev/null 2>&1; then
    echo "ERROR: host tests fail - not packaging" >&2
    make -s -f "$ROOT/Makefile.host" -C "$ROOT" test 2>&1 | grep -v "passed" >&2
    exit 1
fi
echo "host tests: all suites pass"

# Every binary in the package must be newer than every source it is
# built from (see make-release.sh for the release that taught this).
newest=$(find "$ROOT/core" "$ROOT/library" "$ROOT/commodity" "$ROOT/prefs" \
              "$ROOT/include" "$ROOT/tools/esnaptest.c" "$ROOT"/Makefile.* \
              -path "$ROOT/include/aros" -prune -o \
              -type f \( -name '*.c' -o -name '*.h' -o -name '*.conf' \
                         -o -name 'Makefile.*' \) -print0 \
         | xargs -0 stat -f '%m %N' | sort -n | tail -1)
newest_mtime=${newest%% *}
newest_name=${newest#* }
stale=0
for b in EdgeSnap edgesnap.library EdgeSnapPrefs esnaptest; do
    f="$ROOT/build/m68k/$b"
    if [ ! -f "$f" ]; then
        echo "ERROR: build/m68k/$b is missing" >&2
        stale=1
    elif [ "$(stat -f '%m' "$f")" -lt "$newest_mtime" ]; then
        echo "ERROR: build/m68k/$b is older than ${newest_name#$ROOT/}" >&2
        stale=1
    fi
done
if [ "$stale" != "0" ]; then
    echo "       rebuild the lane first: scripts/build-m68k.sh" >&2
    exit 1
fi
echo "binaries: all 4 newer than the newest source (${newest_name#$ROOT/})"

STAGE="$ROOT/build/release-68k"
OUT="$ROOT/build/EdgeSnap-68k-$VERSION.lha"

rm -rf "$STAGE"
mkdir -p "$STAGE/EdgeSnap"
"$ROOT/scripts/stage-package-68k.sh" "$STAGE/EdgeSnap"
cp "$ROOT/assets/EdgeSnapDrawer.info" "$STAGE/EdgeSnap.info"

MANIFEST="
EdgeSnap.info
EdgeSnap/Install
EdgeSnap/Install.info
EdgeSnap/LICENSE
EdgeSnap/EdgeSnap.guide
EdgeSnap/EdgeSnap.guide.info
EdgeSnap/EdgeSnap.readme
EdgeSnap/EdgeSnap.readme.info
EdgeSnap/EdgeSnap.prefs
EdgeSnap/m68k/EdgeSnap
EdgeSnap/m68k/EdgeSnap.info
EdgeSnap/m68k/edgesnap.library
EdgeSnap/m68k/esnaptest
EdgeSnap/m68k/EdgeSnapPrefs
EdgeSnap/m68k/EdgeSnapPrefs.info
"
missing=0
for f in $MANIFEST; do
    if [ ! -s "$STAGE/$f" ]; then
        echo "ERROR: package is missing $f" >&2
        missing=1
    fi
done
extra=$(cd "$STAGE" && find . -type f | sed 's|^\./||' | sort)
for f in $extra; do
    case " $(echo $MANIFEST) " in
        *" $f "*) ;;
        *) echo "ERROR: package contains $f, which is not in the manifest" >&2
           missing=1 ;;
    esac
done
if [ "$missing" != "0" ]; then
    exit 1
fi
echo "manifest: all $(echo $MANIFEST | wc -w | tr -d ' ') files present, nothing extra"

# Koji Arai's lha compresses (-lh5-); the Mac's own is Lhasa, which
# only extracts (see make-release.sh).
LHA_BIN=${LHA_BIN:-"$HOME/amiga-dev/tools/lha-src/src/lha"}
rm -f "$OUT"
if [ -x "$LHA_BIN" ]; then
    ( cd "$STAGE" && "$LHA_BIN" a "$OUT" EdgeSnap.info EdgeSnap >/dev/null )
    echo "packed $(basename "$OUT") with $LHA_BIN (compressed)"
else
    python3 "$ROOT/scripts/make-lha.py" "$OUT" "$STAGE" EdgeSnap.info EdgeSnap
    echo "WARNING: no LhA encoder at $LHA_BIN - the archive is STORED" >&2
fi
check="$ROOT/build/release-68k-check"
rm -rf "$check"
mkdir -p "$check"
(cd "$check" && lha xfq "$OUT" >/dev/null 2>&1) || true
if diff -r "$STAGE" "$check" >/dev/null 2>&1; then
    echo "verified: $(basename "$OUT") unpacks byte for byte"
else
    echo "ERROR: $(basename "$OUT") does not unpack to what went in" >&2
    diff -r "$STAGE" "$check" | head -10 >&2
    exit 1
fi
ls -l "$OUT"
