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
    ("Window snapping for AmigaOS 4.x, MorphOS and AROS x86_64\n",
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
open. S:User-Startup only gets a comment saying so.
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
3.2, and it runs on any 68000 or later, with or without a graphics
card.
"""),
    ("""To remove it: delete the two files above and the line between
@{b};BEGIN EdgeSnap@{ub} and @{b};END EdgeSnap@{ub} in S:User-Startup.
On AROS delete the library and the two files in SYS:WBStartup.
""",
     """To remove it: delete the library, the two files in SYS:WBStartup
and the two in SYS:Prefs.
"""),

    # the corners
    ("""corner, then release. The zones are: left and right edges for halves,
the four corners for quarters, the top edge to maximise.
""",
     """corner, then release. The zones are: left and right edges for halves,
the four corners for quarters, the top edge to maximise.

A corner is the corner itself. Anywhere along a side edge gives the
half, even with the title bar right under the screen's own; for a
quarter, push the pointer on into the corner, where the screen stops
it. The top edge is the screen's own title bar, for the same reason: a
window that sits at the top is not maximised the moment it is touched.
If you would rather have longer corners, and a top edge that reaches
into the screen, the Corner size setting gives them back.
"""),

    # windows that stop at the edge
    ("""@{b}Windows that may not leave the screen@{ub}
On MorphOS the IControl preferences decide whether a window may be
dragged past the edge of the screen. As delivered they say no, except
while Control is held, and a window that stops at the edge keeps the
pointer where it grabbed the title bar: the pointer never touches the
edge, so nothing snaps. Either hold Control while you drag, or open
@{b}SYS:Prefs/IControl@{ub} and allow positioning beyond the screen
limits. AROS has the same switch in its IControl preferences, on by
default; AmigaOS 4 lets a window leave the screen without asking.
""",
     """@{b}Windows that may not leave the screen@{ub}
AmigaOS 3.2 as installed keeps windows on the screen: one dragged
against an edge stops there, and the pointer stops with it, where it
holds the title bar. It never reaches the edge, so a drag never snaps;
the hotkeys and the selector still do.

So the first time EdgeSnap is installed, the installer turns on the
setting of AmigaOS that lets windows move off-screen.
@{b}Restart the system after that first installation@{ub}: AmigaOS
takes the setting up when it starts, and until then a dragged window
does not snap. The installer says so as well. It is the box
@{b}Can Move Off-screen@{ub} in @{b}SYS:Prefs/IControl@{ub}, and
EdgeSnap's own preferences have it too, as
@{b}Windows can move off-screen@{ub}: that is where to turn it back.

Changing it makes AmigaOS ask to reset the Workbench screen, naming the
windows to close first. Close them and press Retry, or press Cancel and
let it take effect at the next start.
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
@{b}Run >NIL: <NIL:@{ub}) and it prints what it decides, drag by drag -
useful when something does not behave as expected.
""",
     """Run from a Shell rather than from SYS:WBStartup and it prints what
it decides, drag by drag - useful when something does not behave as
expected. Stop the copy Workbench started first, with
@{b}EdgeSnap QUIT@{ub} or from Exchange.
"""),

    # for programmers
    ("""EdgeSnap is 0.4: a beta, complete in what it does and tested on real
MorphOS hardware and on AmigaOS 4 and AROS under emulation.
@{b}edgesnap.library@{ub} says 2.17, and that is not a mistake: a
""",
     """EdgeSnap is 0.4: a beta, complete in what it does and tested on
AmigaOS 3.2, on real hardware and under emulation.
@{b}edgesnap.library@{ub} says 2.20, and that is not a mistake: a
"""),
    ("""@{b}esnaptest@{ub}, in the os4, mos and aros64 drawers of this package,
is a small client""",
     """@{b}esnaptest@{ub}, in the m68k drawer of this package,
is a small client"""),
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
