<!-- This README is the classic lane's own. At a merge from EdgeSnap keep
     this version, and carry over by hand only what applies to the 68k. -->
# EdgeSnap classic

**Author:** Michele Dipace <michele.dipace@kaffeine.net>
**License:** MIT (see [LICENSE](LICENSE))

Window snapping for the classic Amiga, as on Windows and macOS. Drag a
window by its title bar until the pointer touches a screen edge or
corner: a frame shows where it will land, and letting go fills that
half or quarter of the screen, or all of it at the top edge.

It runs on AmigaOS 3.1 and later (it starts on 2.04), on any 68000 or
later, with or without a graphics card. Written in C89, with no system
patches.

![The preferences window on AmigaOS 3.2](docs/screenshots/prefs-amigaos3.png)

*The preferences window on AmigaOS 3.2.3 (GadTools, 640x256).*

This is the 68k lane of [EdgeSnap](https://github.com/kaffeine1/edgesnap),
which runs on AmigaOS 4, MorphOS and AROS. The engine, the library and
the shared code come from that repository by merge.

<!-- patreon-support:start -->
## Support development

EdgeSnap remains free and open source. You can [support my development work on Patreon](https://www.patreon.com/c/kaffeine1) to help fund testing, documentation and time spent improving it. No subscription is required to use the software, and public code, releases and articles remain available to everyone.

If you prefer a one-time contribution, you can [support my work on Ko-fi](https://ko-fi.com/kaffeine1).
<!-- patreon-support:end -->

## What it does

- Drag to an edge or corner, with a preview frame. Docks and panels are
  left uncovered.
- Two windows side by side share their edge: drag the seam and both are
  resized.
- A snapped window dragged away from the edges gets its old size back.
- Hotkeys, each one configurable: `ctrl alt` with the cursor keys for
  left, right, maximize and restore, `ctrl alt c` for the middle of the
  screen, `ctrl alt z` for a selector of nine cells under the pointer.
- An ARexx port, `EDGESNAP`.
- A commodity, started from `SYS:WBStartup` and controlled from
  Exchange.
- A preferences window in `SYS:Prefs`. The same settings work as icon
  tooltypes, in `ENVARC:EdgeSnap.prefs` and as Shell arguments.

`EdgeSnap.guide`, in the package, documents all of it.

## Installing

Download `edgesnap.m68k-amigaos.lha` from Aminet (`util/cdity`), unpack
it and double-click Install. The library goes to `LIBS:`, the commodity
and its icon to `SYS:WBStartup`, the preferences to `SYS:Prefs`.
`S:User-Startup` is not touched. Updating is installing again, with no
reboot.

**On AmigaOS 3.2, restart after the first installation.** AmigaOS 3.2
keeps windows on the screen as installed, and a window dragged against
an edge holds the pointer back. The installer turns on IControl's "Can
Move Off-screen", which takes effect at the next start. EdgeSnap's
preferences can turn it off again.

Where windows stop at the edge (on 3.1 and earlier, or with that
setting off) push the mouse a little further against the edge and the
frame comes up. The distance is "Push against an edge" (`PUSHPX`, 24
pixels); 0 turns it off.

The glide into place is off as installed, because without a graphics
card it can be slow. On AmiKit, EdgeSnap also snaps the windows MCP
moves whole and leaves MCP's settings alone; the tooltype `KEEPMCP=NO`
turns MCP's moving off while EdgeSnap runs.

## Status

The current release is **0.51** (2026-11-02), with `edgesnap.library`
2.28. It is the first public release of this lane and a beta.

Tested on AmigaOS 3.2.3 under emulation, with AGA and with a graphics
card, and on 3.1 under emulation. Earlier versions ran on real
hardware: a stock A1200 (68EC020, AGA), an A1200 with a Vampire and one
with a PiStorm running AmiKit. AmigaOS 3.1.4 has not been tried yet;
reports are welcome.

A classic release has the version of the EdgeSnap release it comes
from. The library is numbered above that release's library, and
`include/edgesnap_types.h` lists which 2.x versions were classic.

## Differences from the other systems

- A 68k `edgesnap.library`, with the jump table of
  `library/morphos/edgesnap_lib.fd` and register gates
  (`library/m68k/`).
- Preferences in GadTools (`prefs/gadtools/`), so they run on 3.1 with
  nothing added.
- "Can Move Off-screen" turned on at the first installation. The push
  against an edge counts only where windows cannot leave the screen.
- The preview frame is drawn by the CPU into screen memory in the
  accent colour, on AGA and graphics cards alike
  (`commodity/frame_bits.c`).
- The glide off as installed. On AmiKit, while MCP moves a window
  whole, the frame is four small windows; `KEEPMCP=NO` turns MCP's
  moving off instead (`commodity/mcp_config.c`).
- The guide is the shared one with the passages that differ replaced
  at build time (`scripts/derive-68k-guide.py`). A shared passage that
  changes stops the build until the replacement is updated.

## Building

- [m68k-amigaos-gcc](https://github.com/AmigaPorts/m68k-amigaos-gcc)
  6.5 with libnix and the NDK 3.2, plus `fd2sfd` and `sfdc`, in
  `~/amiga-dev/toolchain/bin` or on the `PATH`.
- `scripts/build-m68k.sh` builds the library, the commodity, the
  preferences and the tools into `build/m68k/` (`Makefile.m68k`).
- `make -f Makefile.host test` runs the core's tests on the host.
- `scripts/make-release-68k.sh` checks the versions, runs the tests and
  writes `build/EdgeSnap-68k-<version>.lha`.

The library has no startup code and no C runtime; only the 68000's
32-bit multiply and divide come from libnix. The commodity and the
preferences use libnix's startup, without ixemul.library. One binary
runs on the 68000 and later; `M68K_CPU=68020` builds a faster one.

## For programmers

`edgesnap.library` places windows, reports zones and the usable area,
and finds and moves the seam between two tiled windows. Its calls are
the same on every system. The header is
[include/edgesnap.h](include/edgesnap.h), the guide has a page for
programmers, and [tools/esnaptest.c](tools/esnaptest.c) (in the
package as `esnaptest`) drives the whole API.
[docs/DESIGN.md](docs/DESIGN.md) is the shared design document.

## Layout

- `core/`: portable C89, tested on the host: zones, the drag engine,
  the snap registry, docks, seams, settings.
- `include/`: the public headers.
- `library/edgesnap_body.c`: the implementation of the API;
  `library/m68k/` makes it a 68k library.
- `commodity/`: the commodity, the frame and the MCP handling.
- `prefs/gadtools/`: the preferences window.
- `catalogs/`: the catalog description and the Italian translation.
- `installer/Install`: the Installer script, shared with the other
  systems.
- `tools/`: `esnaptest` and the test bench tools.
- `scripts/`: `build-m68k.sh`, `make-release-68k.sh`,
  `stage-package-68k.sh`, `derive-68k-guide.py`.

The other Makefiles, `library/os4`, `library/morphos`, `library/aros`
and the other preferences arrive with the merges and are not part of
the 68k build.

## Rules for the code

- The input handler runs in input.device's context: it copies event
  facts, counts and signals the main task. All Intuition calls are made
  in the task.
- Under `LockIBase()` only reads; the lock is released before any
  Intuition call. A stored window pointer is checked against the
  screen's window list before every use.

## Prior art

GoSnap, a commodity for AmigaOS 3.1.4 and later, snaps a window dragged
to an edge or corner.

## Thanks

**Carlo Spadoni** drew the icons. `scripts/make-system-icons.py` only
adds the type, the stack, the tooltypes and the planar image older
icon.library versions show.

**Jan Zahurancik** of AmiKit explained to me how to follow a drag when
MCP's solid window moving keeps the press for itself.

## Reports

Open an issue on
[this repository](https://github.com/kaffeine1/edgesnap-classic/issues)
with the machine, the system version and what you did.
