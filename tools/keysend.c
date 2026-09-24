/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * keysend - put input events into the system as if a hand had made
 * them, through commodities.library. The 68k bench runs in an emulator
 * whose window takes no synthetic input from the host, so the tests
 * press the hotkeys and move the mouse from inside:
 *
 *   keysend KEY "<ctrl alt cursor_left>"   a key, in hotkey notation
 *   keysend AT x y [HELD]                  the pointer, at a screen pixel;
 *                                          HELD while the button is down
 *   keysend MOVE dx dy [HELD]              the mouse, relative (Intuition
 *                                          on the bench ignored these)
 *   keysend RAW x y [HELD]                 IECLASS_RAWMOUSE with x, y as
 *                                          an absolute position, no
 *                                          relative qualifier
 *   keysend DRAG x0 y0 x1 y1 [steps]       press at (x0, y0), move to
 *                                          (x1, y1) in steps, release:
 *                                          a whole title-bar drag
 *   keysend BUTTON DOWN | UP               the left button, as the mouse
 *                                          reports it (IECLASS_RAWMOUSE)
 *   keysend TBUTTON DOWN | UP              the same, as a tablet does
 *   keysend WHERE                          print the pointer's position
 *   keysend MODES                          list the display modes: id,
 *                                          size, depth, name
 *
 * What the bench taught (2026-09-24): AT, IECLASS_NEWPOINTERPOS with a
 * pixel position on the Workbench screen, puts the pointer exactly
 * there, as WinUAE's mouse hack does; but a window drag follows only
 * IECLASS_RAWMOUSE events, which Intuition takes as RELATIVE steps
 * with the Y in input units, twice the pixels of a non-interlaced
 * screen, and only without IEQUALIFIER_RELATIVEMOUSE set (with it
 * they moved nothing). DRAG puts that together: AT for the start, raw
 * steps with the button qualifier held, raw button events with no
 * movement of their own.
 *
 * HELD matters: Intuition drags a window only when the movement events
 * say the button is down (IEQUALIFIER_LEFTBUTTON), as input.device
 * says it on every event while a real button is held. Without it the
 * pointer moves and the window stays (2026-09-24).
 *
 * Every event goes down the same input pipeline the commodity listens
 * on, so what it sees is what it would see from a keyboard.
 */
#include <exec/types.h>
#include <devices/inputevent.h>
#include <libraries/commodities.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/commodities.h>
#include <intuition/intuitionbase.h>
#include <proto/intuition.h>
#include <graphics/displayinfo.h>
#include <proto/graphics.h>
#include <clib/alib_protos.h>   /* InvertString, FreeIEvents: amiga.lib */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Library *CxBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

static void modes(void)
{
    ULONG id = INVALID_ID;

    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 39);
    if (GfxBase == NULL) {
        return;
    }
    while ((id = NextDisplayInfo(id)) != INVALID_ID) {
        struct DimensionInfo di;
        struct NameInfo ni;
        DisplayInfoHandle h = FindDisplayInfo(id);

        if (h == NULL) {
            continue;
        }
        if (GetDisplayInfoData(h, (UBYTE *)&di, sizeof(di), DTAG_DIMS, id) <= 0) {
            continue;
        }
        if (GetDisplayInfoData(h, (UBYTE *)&ni, sizeof(ni), DTAG_NAME, id) <= 0) {
            ni.Name[0] = '\0';
        }
        printf("%08lx %4dx%-4d depth %2d  %s\n", (unsigned long)id,
               (int)(di.Nominal.MaxX - di.Nominal.MinX + 1),
               (int)(di.Nominal.MaxY - di.Nominal.MinY + 1),
               (int)di.MaxDepth, (char *)ni.Name);
    }
    CloseLibrary((struct Library *)GfxBase);
    GfxBase = NULL;
}

static int same(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = *a, cb = *b;
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == *b;
}

static void at(WORD x, WORD y, UWORD code, int held)
{
    struct InputEvent ev;
    struct IEPointerPixel pp;
    struct Screen *scr;

    IntuitionBase = (struct IntuitionBase *)
        OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    if (IntuitionBase == NULL) {
        return;
    }
    scr = LockPubScreen(NULL);
    if (x < 0) {                           /* "where it is": buttons */
        x = IntuitionBase->MouseX;
        y = IntuitionBase->MouseY;
    }
    pp.iepp_Screen = scr;
    pp.iepp_Position.X = x;
    pp.iepp_Position.Y = y;
    memset(&ev, 0, sizeof(ev));
    ev.ie_Class = IECLASS_NEWPOINTERPOS;
    ev.ie_SubClass = IESUBCLASS_PIXEL;
    ev.ie_Code = code;
    ev.ie_Qualifier = held ? IEQUALIFIER_LEFTBUTTON : 0;
    ev.ie_EventAddress = &pp;
    AddIEvents(&ev);
    Delay(5);                 /* the event names pp: let it be consumed */
    if (scr != NULL) {
        UnlockPubScreen(NULL, scr);
    }
    CloseLibrary((struct Library *)IntuitionBase);
    IntuitionBase = NULL;
}

static void mouse(UWORD code, WORD dx, WORD dy, int held)
{
    struct InputEvent ev;

    memset(&ev, 0, sizeof(ev));
    ev.ie_Class = IECLASS_RAWMOUSE;
    ev.ie_Code = code;
    ev.ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE |
                      (held ? IEQUALIFIER_LEFTBUTTON : 0);
    ev.ie_X = dx;
    ev.ie_Y = dy;
    AddIEvents(&ev);
}

/* A raw mouse event carrying a position, the way input.device hands
 * them on: absolute, no relative qualifier. For buttons x < 0 means
 * "where the pointer is now". */
static void raw(WORD x, WORD y, UWORD code, int held);

static void drag(WORD x0, WORD y0, WORD x1, WORD y1, int steps)
{
    int i;
    long sx = 0, sy = 0;           /* what has been sent, in pixels */

    if (steps < 1) {
        steps = 8;
    }
    at(x0, y0, IECODE_NOBUTTON, 0);
    Delay(5);
    raw(0, 0, IECODE_LBUTTON, 1);
    Delay(5);
    for (i = 1; i <= steps; i++) {
        long tx = (long)(x1 - x0) * i / steps;
        long ty = (long)(y1 - y0) * i / steps;

        raw((WORD)(tx - sx), (WORD)((ty - sy) * 2), IECODE_NOBUTTON, 1);
        sx = tx;
        sy = ty;
    }
    Delay(5);
    raw(0, 0, IECODE_LBUTTON | IECODE_UP_PREFIX, 0);
    Delay(10);
}

static void raw(WORD x, WORD y, UWORD code, int held)
{
    struct InputEvent ev;
    struct Screen *scr = NULL;

    if (x < 0) {
        IntuitionBase = (struct IntuitionBase *)
            OpenLibrary((CONST_STRPTR)"intuition.library", 39);
        if (IntuitionBase != NULL) {
            scr = LockPubScreen(NULL);
            if (scr != NULL) {
                x = scr->MouseX;
                y = scr->MouseY;
                UnlockPubScreen(NULL, scr);
            }
            CloseLibrary((struct Library *)IntuitionBase);
            IntuitionBase = NULL;
        }
    }
    memset(&ev, 0, sizeof(ev));
    ev.ie_Class = IECLASS_RAWMOUSE;
    ev.ie_Code = code;
    ev.ie_Qualifier = held ? IEQUALIFIER_LEFTBUTTON : 0;
    ev.ie_X = x;
    ev.ie_Y = y;
    AddIEvents(&ev);
    Delay(5);
}

int main(int argc, char **argv)
{
    int rc = 0;

    if (argc < 2) {
        printf("keysend KEY \"<desc>\" | MOVE dx dy | BUTTON DOWN|UP\n");
        return 5;
    }
    CxBase = OpenLibrary((CONST_STRPTR)"commodities.library", 37);
    if (CxBase == NULL) {
        printf("keysend: no commodities.library\n");
        return 20;
    }
    if (same(argv[1], "KEY") && argc >= 3) {
        struct InputEvent *ie = InvertString((CONST_STRPTR)argv[2], NULL);

        if (ie != NULL) {
            AddIEvents(ie);
            FreeIEvents(ie);
        } else {
            printf("keysend: cannot parse '%s'\n", argv[2]);
            rc = 10;
        }
    } else if (same(argv[1], "AT") && argc >= 4) {
        at((WORD)atoi(argv[2]), (WORD)atoi(argv[3]), IECODE_NOBUTTON,
           argc >= 5 && same(argv[4], "HELD"));
    } else if (same(argv[1], "DRAG") && argc >= 6) {
        drag((WORD)atoi(argv[2]), (WORD)atoi(argv[3]),
             (WORD)atoi(argv[4]), (WORD)atoi(argv[5]),
             argc >= 7 ? atoi(argv[6]) : 8);
    } else if (same(argv[1], "RAW") && argc >= 4) {
        raw((WORD)atoi(argv[2]), (WORD)atoi(argv[3]), IECODE_NOBUTTON,
            argc >= 5 && same(argv[4], "HELD"));
    } else if (same(argv[1], "RBUTTON") && argc >= 3) {
        /* no movement of its own: a raw event's x, y are a step */
        if (same(argv[2], "DOWN")) {
            raw(0, 0, IECODE_LBUTTON, 1);
        } else {
            raw(0, 0, IECODE_LBUTTON | IECODE_UP_PREFIX, 0);
        }
    } else if (same(argv[1], "MOVE") && argc >= 4) {
        mouse(IECODE_NOBUTTON, (WORD)atoi(argv[2]), (WORD)atoi(argv[3]),
              argc >= 5 && same(argv[4], "HELD"));
    } else if (same(argv[1], "MODES")) {
        modes();
    } else if (same(argv[1], "WHERE")) {
        IntuitionBase = (struct IntuitionBase *)
            OpenLibrary((CONST_STRPTR)"intuition.library", 36);
        if (IntuitionBase != NULL) {
            struct Screen *scr = LockPubScreen(NULL);

            /* IntuitionBase's pair is in the input's own units (the Y
             * of a non-interlaced screen counts double); the screen's
             * pair is in its pixels, which is what a window sees. */
            printf("base %d %d  screen %d %d\n",
                   (int)IntuitionBase->MouseX, (int)IntuitionBase->MouseY,
                   scr ? (int)scr->MouseX : -1, scr ? (int)scr->MouseY : -1);
            if (scr != NULL) {
                UnlockPubScreen(NULL, scr);
            }
            CloseLibrary((struct Library *)IntuitionBase);
        }
    } else if (same(argv[1], "BUTTON") && argc >= 3) {
        if (same(argv[2], "DOWN")) {
            mouse(IECODE_LBUTTON, 0, 0, 1);
        } else {
            mouse(IECODE_LBUTTON | IECODE_UP_PREFIX, 0, 0, 0);
        }
        Delay(5);
    } else if (same(argv[1], "TBUTTON") && argc >= 3) {
        if (same(argv[2], "DOWN")) {
            at(-1, -1, IECODE_LBUTTON, 1);
        } else {
            at(-1, -1, IECODE_LBUTTON | IECODE_UP_PREFIX, 0);
        }
    } else {
        printf("keysend: what?\n");
        rc = 5;
    }
    CloseLibrary(CxBase);
    return rc;
}
