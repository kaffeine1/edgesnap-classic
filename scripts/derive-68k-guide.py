#!/usr/bin/env python3
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
"""The classic guide's template, made from the one the other systems use.

A copy kept by hand falls behind at every release of the other systems,
as it did at 0.4: the selector's hotkey and the page for programmers
changed there and not here. So the classic template is made at build
time from packaging/EdgeSnap.guide.in, replacing only the passages that
say something different on AmigaOS 3.x, and each replacement must match
exactly once: a passage that changed upstream stops the build, and gets
looked at, instead of slipping through in the wrong wording.

  derive-68k-guide.py <EdgeSnap.guide.in> <out>
"""
import sys

PAIRS = [
    # the systems
    ("Window snapping for AmigaOS 4.x, MorphOS and AROS (x86_64, aarch64, i386)\n",
     "Window snapping for AmigaOS 3.1 and later, on the 68k\n"),

    # what the installer puts where
    ("""  @{b}LIBS:edgesnap.library@{ub}   everything EdgeSnap knows how to do
  @{b}C:EdgeSnap@{ub}              the commodity you interact with
  @{b}ENVARC:EdgeSnap.prefs@{ub}   the settings, commented, changing nothing
  one line in @{b}S:User-Startup@{ub}   so it is simply there at every boot
  @{b}SYS:Prefs/EdgeSnap@{ub}      the settings window
  the manual, where you say

On AROS the commodity goes to @{b}SYS:WBStartup/EdgeSnap@{ub} with its
icon instead, and Wanderer starts it at every boot: a Run line in
S:User-Startup would open a console window for it there and keep it
open. S:User-Startup is not touched.

On AROS for the Raspberry Pi and on 32-bit AROS there is no Installer
to go through: Install does the work itself, as it does on any AROS
whose own Installer cannot copy files. One requester says what goes where and
asks; the manual goes to SYS:Documentation, or SYS:Locale/Help where
there is none; and no settings file is written, since the preferences
window writes one when you press Save.
""",
     """  @{b}LIBS:edgesnap.library@{ub}    everything EdgeSnap knows how to do
  @{b}SYS:WBStartup/EdgeSnap@{ub}   the commodity you interact with, with
                            its icon, so Workbench starts it at every boot
  @{b}ENVARC:EdgeSnap.prefs@{ub}    the settings, commented, changing nothing
  @{b}SYS:Prefs/EdgeSnap@{ub}       the settings window
  the manual, where you say

Nothing is written to S:User-Startup: the commodity starts from
SYS:WBStartup like any other, and the icon's tooltypes are read as
Workbench starts it. It needs AmigaOS 2.04 or later; it was tested on
3.2 and 3.1, and it runs on any 68000 or later, with or without a
graphics card.
"""),
    ("""and nothing for you to type. On the Raspberry Pi the old library cannot
be let go of yet, so a new one takes over at the next boot. Your
settings are left alone: the commented template is only offered when
ENVARC:EdgeSnap.prefs does not exist yet, and a file already there is
never touched.
""",
     """and nothing for you to type. Your settings are left alone: the
commented template is only offered when ENVARC:EdgeSnap.prefs does not
exist yet, and a file already there is never touched.
"""),
    ("""To remove it: delete the two files above and the line between
@{b};BEGIN EdgeSnap@{ub} and @{b};END EdgeSnap@{ub} in S:User-Startup.
On AROS delete the library and the two files in SYS:WBStartup.
""",
     """To remove it: delete the library, the two files in SYS:WBStartup
and the two in SYS:Prefs.
"""),

    # windows that stop at the edge
    ("""@{b}Windows that may not leave the screen@{ub}
A drag snaps where the pointer reaches the edge. Whether a dragged
window may go on past the edge is the system's own setting: in IControl
on MorphOS and AROS, in the GUI preferences on AmigaOS 4 (off-screen
dragging). Where it may not, the window stops at the edge and holds
the pointer back where it has the title bar. Push the mouse on a little
further against the edge and EdgeSnap counts the pointer as being
there: the frame comes up and stays while you hold. For a corner, push
on into both edges, towards the corner. AROS drags a window as an
outline, and there the push counts where the outline is held at the
edge. The zones still answer more directly when the window can go on.

How far you push is @{b}PUSHPX@{ub}, "Push against an edge" in the
preferences (24 pixels as it comes). The same push answers on AmigaOS
4 when the GUI preferences give the screen edges a resistance: the
window rests at the edge for a while before it goes on. If you only
want to lay a window against the edge there, set it to 0 to turn the
push off: a window then snaps only once the pointer itself reaches the
edge, after the window has gone past the resistance. Where windows may
not leave the screen at all, 0 leaves a drag nothing to snap to; the
hotkeys still snap.

MorphOS as delivered keeps windows on the screen, so the first
installation there chooses IControl's "allow positioning beyond the
screen limits", says so on a page of its own, and MorphOS takes it up
at once. An update leaves the setting as it finds it. AROS and
AmigaOS 4 let a window go past the edge as delivered, unless someone
turned it off.

To change it either way, use the box @{b}Windows can move off-screen@{ub}
in EdgeSnap's preferences, in the section named after the system, or
the system's own editor: IControl, or GUI on AmigaOS 4. On MorphOS,
IControl's key, Control as delivered, turns the setting round while it
is held.
""",
     """@{b}Windows that may not leave the screen@{ub}
AmigaOS 3.2 as installed keeps windows on the screen: one dragged
against an edge stops there, and the pointer stops with it, where it
holds the title bar, short of the edge.

So the first time EdgeSnap is installed, the installer turns on the
setting of AmigaOS that lets windows move off-screen, and the pointer
goes on to the edge with them.
@{b}Restart the system after that first installation@{ub}: AmigaOS
takes the setting up when it starts. The installer says so as well.
It is the box @{b}Can Move Off-screen@{ub} in
@{b}SYS:Prefs/IControl@{ub}, and EdgeSnap's own preferences have it
too, as @{b}Windows can move off-screen@{ub}: that is where to turn it
back.

Changing it makes AmigaOS ask to reset the Workbench screen, naming the
windows to close first. Close them and press Retry, or press Cancel and
let it take effect at the next start.

Where windows stop at the edge, until that restart, with the setting
turned off, and on AmigaOS 3.1 and earlier, which do not have it,
EdgeSnap counts the mouse's own travel instead. Drag the window against
the edge, push the mouse on a little further, and the frame comes up
and stays while you hold. For a corner, push on into both edges,
towards the corner.

How far you push is @{b}PUSHPX@{ub}, "Push against an edge" in the
preferences (24 pixels as it comes). Set it to 0 to turn the push off:
a drag then snaps only where the pointer itself reaches the edge, so
where windows stop at the edge it has nothing to snap to; the hotkeys
still snap. Where windows move off-screen the push does not count,
whatever PUSHPX says: the pointer gets to the edge by itself.

@{b}MCP and windows that move whole@{ub}
MCP, part of AmiKit, can move and size windows itself, contents and
all: "Finestre piene: sposta" and "ridimensiona" in its preferences. It
then keeps every press on a title bar for itself, and EdgeSnap never
sees a drag begin. So while EdgeSnap runs it turns those two off, in
ENV: only, and back on when it stops: MCP's saved preferences stay as
they are. The tooltype @{b}KEEPMCP=YES@{ub} leaves MCP alone; a window
that MCP moves is then not snapped when it reaches an edge.

@{b}CX_PRIORITY@{ub}, the tooltype every commodity takes, sets where
EdgeSnap hears the mouse among the other commodities: 0 as it comes.
Raise it if another commodity keeps the mouse button for itself.
"""),

    # the preferences window: on AmigaOS the setting waits
    ("""A refused hotkey leaves the window open for correction. It also has the
system's own "Windows can move off-screen", in a section named after
the system (see above): it is kept in the system's preferences, not in
EdgeSnap's, and the system follows it at once.
""",
     """A refused hotkey leaves the window open for correction. It also has the
system's own "Windows can move off-screen", in the section named
AmigaOS (see above): it is kept in the system's preferences, not in
EdgeSnap's, and AmigaOS takes it up when it resets the Workbench
screen or starts again.
"""),

    # the seam
    ("""preview frame. Press on it and drag, and both windows are resized
together; on AmigaOS 4 and MorphOS the pointer becomes a double arrow
and the windows follow as you drag, on AROS only the line follows and
the windows change when you let go, the way AROS moves its own
windows.""",
     """preview frame. Press on it and drag, and both windows are resized
together, following the pointer as you drag."""),

    # running it from a Shell
    ("""Run from a Shell rather than with @{b}Run >NIL:@{ub} (on AROS
@{b}Run >NIL: <NIL:@{ub}) and it prints what it decides, drag by drag,
which helps when something does not behave as expected.
""",
     """Run from a Shell rather than from SYS:WBStartup and it prints what
it decides, drag by drag, which helps when something does not behave as
expected. Stop the copy Workbench started first, with
@{b}EdgeSnap QUIT@{ub} or from Exchange.
"""),

    # the ARexx port: what the other systems need is said for them
    ("""ARexx itself must be installed and running (RexxMast); EdgeSnap only
needs rexxsyslib.library to open its port, and works without it. On
MorphOS that means the ARexx interpreter installed on top of the
system's own rexxsyslib, which has none. On AROS One the command is
@{b}SYS:Rexxc/RX@{ub}, and its RC after a command to a port is not to be
trusted: the trace shows the real code, and RESULT works as
everywhere.
""",
     """ARexx itself must be running: @{b}SYS:System/RexxMast@{ub}, by hand
or from SYS:WBStartup. EdgeSnap only needs rexxsyslib.library to open
its port, and works without it. From a Shell a script runs with
@{b}SYS:Rexxc/RX@{ub}.
"""),

    # for programmers
    ("""EdgeSnap is 0.51: a beta, complete in what it does and tested on real
MorphOS hardware, on a Raspberry Pi 400 with AROS, and on AmigaOS 4,
MorphOS, AROS x86_64 and AROS i386 under emulation.
@{b}edgesnap.library@{ub} says %%LIBVER%%, and that is not a mistake: a
""",
     """EdgeSnap is 0.51: a beta, complete in what it does and tested on
AmigaOS 3.2, on real hardware and under emulation, and on 3.1 under
emulation.
@{b}edgesnap.library@{ub} says %%LIBVER%%, and that is not a mistake: a
"""),
    ("""@{b}esnaptest@{ub}, in this package's drawer for each system (os4, mos,
aros64, aarch64 or i386), is a small client that opens the library and
drives the whole API""",
     """@{b}esnaptest@{ub}, in the m68k drawer of this package, is a small
client that opens the library and drives the whole API"""),

    # the reports go to this lane's own repository
    ("  https://github.com/kaffeine1/edgesnap/issues\n",
     "  https://github.com/kaffeine1/edgesnap-classic/issues\n"),
]


def main(argv):
    if len(argv) != 3:
        sys.stderr.write(__doc__)
        return 2
    text = open(argv[1], encoding="latin-1").read()
    for old, new in PAIRS:
        n = text.count(old)
        if n != 1:
            sys.stderr.write("derive-68k-guide: a passage matches %d times, "
                             "not once - the guide changed upstream:\n%s\n"
                             % (n, old[:160]))
            return 1
        text = text.replace(old, new)
    for word in ("MorphOS", "AROS", "AmigaOS 4"):
        if word in text:
            at = text.index(word)
            line = text[:at].count("\n") + 1
            sys.stderr.write("derive-68k-guide: '%s' still in the classic "
                             "guide, line %d\n" % (word, line))
            return 1
    open(argv[2], "w", encoding="latin-1").write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
