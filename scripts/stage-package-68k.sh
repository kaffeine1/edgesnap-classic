#!/bin/sh
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Fill a directory with the classic package, the one for AmigaOS 3.1
# and later on 68k, in the shape of the other systems' package
# (scripts/stage-package.sh), so the same Installer script serves:
#
#   Install (+ icon)        the Installer - double-click it
#   EdgeSnap.guide (+icon)  the documentation, in its classic wording
#   EdgeSnap.readme (+icon) the Aminet-style summary
#   EdgeSnap.prefs          the commented settings template (no icon)
#   m68k/                   the build, picked by the installer
#
# The programs themselves are deliberately without icons at the top:
# the installer knows where they go.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DEST="${1:?usage: stage-package-68k.sh <directory>}"

for f in EdgeSnap edgesnap.library EdgeSnapPrefs esnaptest; do
    if [ ! -f "$ROOT/build/m68k/$f" ]; then
        echo "ERROR: build/m68k/$f missing - run scripts/build-m68k.sh first" >&2
        exit 1
    fi
done

mkdir -p "$DEST/m68k"
cp "$ROOT/build/m68k/EdgeSnap"          "$DEST/m68k/EdgeSnap"
cp "$ROOT/build/m68k/edgesnap.library"  "$DEST/m68k/edgesnap.library"
cp "$ROOT/build/m68k/EdgeSnapPrefs"     "$DEST/m68k/EdgeSnapPrefs"
cp "$ROOT/build/m68k/esnaptest"         "$DEST/m68k/esnaptest"
# Carlo Spadoni's icons, in the classic form: the AmigaOS 4 ones with
# IMAG copies inside for icon.library 44 and later, and the planar
# image in front for the older ones (scripts/make-system-icons.py).
# The commodity's holds DONOTWAIT and every setting as a commented
# tooltype: it is what Workbench starts from SYS:WBStartup.
cp "$ROOT/assets/m68k/EdgeSnap.info"      "$DEST/m68k/EdgeSnap.info"
cp "$ROOT/assets/m68k/EdgeSnapPrefs.info" "$DEST/m68k/EdgeSnapPrefs.info"

cp "$ROOT/LICENSE"                       "$DEST/LICENSE"
cp "$ROOT/installer/Install"             "$DEST/Install"
# Carlo Spadoni's pictures for Install, the guide and the readme, the
# same icons as the other systems' package: his ARGB with IMAG copies,
# which icon.library 44 and later show on a classic system too.
cp "$ROOT/assets/Install.info"           "$DEST/Install.info"
cp "$ROOT/assets/EdgeSnap.guide.info"    "$DEST/EdgeSnap.guide.info"
cp "$ROOT/assets/EdgeSnap.readme.info"   "$DEST/EdgeSnap.readme.info"

# The classic guide is the other systems' one with the passages that
# differ here replaced (scripts/derive-68k-guide.py), made fresh each
# time so it cannot fall behind.
GUIDE_IN=$(mktemp "${TMPDIR:-/tmp}/edgesnap-68k-guide.XXXXXX")
python3 "$ROOT/scripts/derive-68k-guide.py" \
    "$ROOT/packaging/EdgeSnap.guide.in" "$GUIDE_IN"
# the defaults printed in the guide are the classic lane's: see COMMON
# in Makefile.m68k
GUIDE_CFLAGS="-DES_CONFIG_ANIMATE_DEFAULT=0" \
    "$ROOT/scripts/make-guide.sh" "$DEST/EdgeSnap.guide" "$GUIDE_IN" >/dev/null
rm -f "$GUIDE_IN"

cat > "$DEST/EdgeSnap.readme" <<'EOR'
Short:        Tile windows by dragging them to an edge
Author:       michele.dipace@kaffeine.net (Michele Dipace)
Uploader:     michele.dipace@kaffeine.net (Michele Dipace)
Type:         util/cdity
Version:      0.41 (beta)
Architecture: m68k-amigaos >= 2.0.4
License:      MIT

EdgeSnap gives AmigaOS 3.1 and later the window snapping of Windows and
macOS. Drag a window by its title bar until the POINTER touches a
screen edge or corner: a frame shows where it will land, and letting go
fills that half or quarter of the screen.

  - docks and panels are detected and never covered;
  - two windows side by side share their edge: press the seam and drag
    it, and both are resized;
  - hotkeys for those who prefer them (ctrl alt cursor keys, ctrl alt c
    for the middle of the screen, ctrl alt z for a selector under
    the pointer);
  - it is a commodity: it starts with the system from SYS:WBStartup,
    and Exchange enables, disables or removes it like any other.

The behaviour lives in edgesnap.library, not in the commodity, so any
program can ask for the same things - the commodity is a client like
any other. esnaptest, in the package, is a worked example.

A window jumps into its zone. It can glide there instead, as on the
other systems, and that is off as installed: 68k machines differ
widely in speed, and without a graphics card a glide can be slow. Turn
on "Glide into place" in the preferences if yours is quick enough.

This build is for the 68000 and later, on AmigaOS 2.04 or later, with
or without a graphics card. It was tested on AmigaOS 3.2.3; 3.1 and
3.1.4 should be fine, and I would like to hear how they do.

THIS IS A BETA for a few testers. Please write to me about anything
that looks wrong, with the machine, the system version and what you
did: michele.dipace@kaffeine.net

TO INSTALL: double-click the Install icon. It recognises the system,
proposes the matching build, and puts everything where it belongs:
the library in LIBS:, the commodity and its icon in SYS:WBStartup, the
preferences in SYS:Prefs, the manual where you say. Nothing is written
to S:User-Startup. Updating is installing again: no reboot needed.

AFTER THE FIRST INSTALLATION, RESTART THE SYSTEM. The installer turns
on one setting of AmigaOS, "Can Move Off-screen" of the IControl
preferences, which AmigaOS takes up when it starts: without it a
dragged window stops at the edge and never snaps. Until the restart the
hotkeys work and dragging does not. The installer says so, and
EdgeSnap's preferences turn the setting back.

A corner is now the corner itself. If you saved EdgeSnap's preferences
with an earlier version, they may keep the corner size they had then,
4, a band along each side: set Corner size to 0 in SYS:Prefs/EdgeSnap
for the corners as they come now.

The icons are by Carlo Spadoni, who drew them for EdgeSnap.

Full documentation is in EdgeSnap.guide.
EOR

# The template is the same as the one scripts/stage-package.sh writes
# for the other systems: the settings are the same words everywhere.
cat > "$DEST/EdgeSnap.prefs" <<'EOR'
# EdgeSnap preferences
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net> - MIT.
#
# Copy to ENVARC:EdgeSnap.prefs (the installer offers to do it). Every
# setting is optional and every line here is commented out, so the file
# changes nothing until you remove a '#'. The same KEY=VALUE words also
# work as Shell arguments:  EdgeSnap ZONES=halves EDGEPX=24
#
# ZONES       which zones react: all | none | halves | corners |
#             left,right,topleft,topright,bottomleft,bottomright,maximize
# EDGEPX      how close to an edge the POINTER must be (default 12)
# CORNERDIV   0: a corner only where the pointer is pushed into it
#             (default); 2..16: a band, usable height / this
# DRAGMINPX   pointer travel before a drag counts (default 4)
# PREVIEW     show the zone preview frame: yes | no
# PANELDETECT reserve dock/panel strips: yes | no
# PANELMARGIN breathing room around a detected dock (default 8)
# MARGINLEFT/TOP/RIGHT/BOTTOM  extra margins of your own (default 0)
# BYPASSQUAL  hold to drag past the zones: none | alt | ctrl | shift
# CYCLESIZES  the same side hotkey again gives two thirds, then one
#             third, then a half again: yes | no
# ANIMATE     glide into the zone instead of jumping: yes | no
#             (default no: how well it looks depends on the machine)

#ZONES=all
#EDGEPX=12
#PREVIEW=yes
#PANELDETECT=yes
#PANELMARGIN=8
#BYPASSQUAL=alt
#CYCLESIZES=yes
#ANIMATE=yes
EOR
