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
 *   keysend SCREENS                        list the screens, which one is
 *                                          Workbench's, and the public
 *                                          ones with the default named
 *   keysend IDRAG x0 y0 x1 y1 [steps] [ADD] [HOLD]
 *                                          a drag written into input.device
 *                                          itself (IND_WRITEEVENT, or with
 *                                          ADD the V47 IND_ADDEVENT); HOLD
 *                                          leaves the button down
 *   keysend IAT x y                        the pointer there, button held
 *   keysend IPUSH dx dy [n]                n relative mouse steps with the
 *                                          button held: a hand pushing a
 *                                          window against an edge
 *   keysend IUP                            the button released
 *   keysend WINBOX "title" x y w h         put the window of that title,
 *                                          on the default public screen,
 *                                          at that place and size
 *   keysend OFFSCREEN                      open a small window that does
 *                                          not take the focus, ask to move
 *                                          it past the right edge of the
 *                                          default public screen, and say
 *                                          whether Intuition let it go:
 *                                          whether IControl's "Can Move
 *                                          Off-screen" is in effect
 *
 * What the bench taught. AT, IECLASS_NEWPOINTERPOS with a pixel
 * position on the Workbench screen, puts the pointer exactly there
 * (2026-09-24). A window drag whose events go through AddIEvents()
 * moves the window by its first step only, whatever the qualifiers,
 * relative or absolute (2026-09-24); why is not known. Written to
 * input.device itself, IDRAG, the same press and pixel positions drag
 * a window all the way, on WinUAE and on real hardware, and the
 * commodity follows the drag to a snap (2026-09-27). With IND_ADDEVENT
 * (V47) input.device also records the button as held, PeekQualifier()
 * says 4000; with IND_WRITEEVENT it does not, and the drag works just
 * the same.
 *
 * Also learnt on 2026-09-27: on AmigaOS 3.2.3 as installed a window
 * dragged against the edge stops there and keeps the pointer where it
 * was grabbed, so the pointer never reaches the edge; IControl's "Can
 * Move Off-screen" lets both go on. MoveWindow() obeys the same
 * setting, so OFFSCREEN tells whether it is in effect without a drag:
 * stopped at the edge on the bench as installed, past it on a machine
 * with the box ticked. A first reading on the bench said "stopped" with
 * the box ticked too, but IPrefs had not applied it: the whole file,
 * copied from another machine, differed in settings that need the
 * Workbench screen reset, and Intuition was waiting on its requester
 * for the windows to close. Change IControl on a machine in use only
 * through its editor, and never on someone else's from afar.
 * A screen blanker's screen in front takes the injected pointer events
 * and is not woken by them; a key is.
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
#include <intuition/screens.h>
#include <devices/input.h>
#include <exec/io.h>
#include <proto/input.h>
#include <proto/graphics.h>
#include <clib/alib_protos.h>   /* InvertString, FreeIEvents: amiga.lib */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Library *CxBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Device *InputBase;

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

/*
 * A drag through input.device rather than past it. A window drag whose
 * events go through AddIEvents() moves by its first step only; written
 * to input.device the same events drag a window all the way (see the
 * notes at the top). One device session per keysend run.
 */
static struct MsgPort *g_iport;
static struct IOStdReq *g_ireq;
static int g_iadd;              /* IND_ADDEVENT rather than IND_WRITEEVENT */
static struct Screen *g_iscr;   /* the default public screen, locked */

static int in_open(int add)
{
    IntuitionBase = (struct IntuitionBase *)
        OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    if (IntuitionBase == NULL) {
        return 0;
    }
    g_iport = CreateMsgPort();
    g_ireq = g_iport ? (struct IOStdReq *)CreateIORequest(g_iport,
                                                          sizeof(*g_ireq))
                     : NULL;
    if (g_ireq == NULL || OpenDevice((CONST_STRPTR)"input.device", 0,
                                     (struct IORequest *)g_ireq, 0) != 0) {
        printf("keysend: input.device would not open\n");
        if (g_ireq != NULL) {
            DeleteIORequest((struct IORequest *)g_ireq);
        }
        if (g_iport != NULL) {
            DeleteMsgPort(g_iport);
        }
        g_ireq = NULL;
        g_iport = NULL;
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = NULL;
        return 0;
    }
    InputBase = g_ireq->io_Device;
    g_iadd = add;
    if (g_iadd && InputBase->dd_Library.lib_Version < 47) {
        printf("keysend: input.device %d has no IND_ADDEVENT, writing "
               "instead\n", (int)InputBase->dd_Library.lib_Version);
        g_iadd = 0;
    }
    g_iscr = LockPubScreen(NULL);
    return 1;
}

static void in_send(struct InputEvent *ev)
{
    g_ireq->io_Command = g_iadd ? IND_ADDEVENT : IND_WRITEEVENT;
    g_ireq->io_Data = (APTR)ev;
    g_ireq->io_Length = sizeof(*ev);
    g_ireq->io_Flags = 0;
    DoIO((struct IORequest *)g_ireq);
}

static void in_close(void)
{
    if (g_iscr != NULL) {
        UnlockPubScreen(NULL, g_iscr);
        g_iscr = NULL;
    }
    CloseDevice((struct IORequest *)g_ireq);
    DeleteIORequest((struct IORequest *)g_ireq);
    DeleteMsgPort(g_iport);
    g_ireq = NULL;
    g_iport = NULL;
    InputBase = NULL;
    CloseLibrary((struct Library *)IntuitionBase);
    IntuitionBase = NULL;
}

/* The pointer at a pixel of the default public screen, the button held
 * or not. The event names pp, so both live as long as the program. */
static void in_at(int x, int y, int held)
{
    static struct InputEvent ev;
    static struct IEPointerPixel pp;

    memset(&ev, 0, sizeof(ev));
    pp.iepp_Screen = g_iscr;
    pp.iepp_Position.X = (WORD)x;
    pp.iepp_Position.Y = (WORD)y;
    ev.ie_Class = IECLASS_NEWPOINTERPOS;
    ev.ie_SubClass = IESUBCLASS_PIXEL;
    ev.ie_Code = IECODE_NOBUTTON;
    ev.ie_Qualifier = held ? IEQUALIFIER_LEFTBUTTON : 0;
    ev.ie_EventAddress = &pp;
    in_send(&ev);
}

/* The left button, as the mouse reports it: no movement of its own. */
static void in_button(int down)
{
    static struct InputEvent ev;

    memset(&ev, 0, sizeof(ev));
    ev.ie_Class = IECLASS_RAWMOUSE;
    ev.ie_Code = down ? IECODE_LBUTTON : (IECODE_LBUTTON | IECODE_UP_PREFIX);
    ev.ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE |
                      (down ? IEQUALIFIER_LEFTBUTTON : 0);
    in_send(&ev);
}

/* The mouse moving by (dx, dy) counts with the button held, as a hand
 * pushing a window against an edge makes it: relative raw events. */
static void in_push(int dx, int dy)
{
    static struct InputEvent ev;

    memset(&ev, 0, sizeof(ev));
    ev.ie_Class = IECLASS_RAWMOUSE;
    ev.ie_Code = IECODE_NOBUTTON;
    ev.ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE | IEQUALIFIER_LEFTBUTTON;
    ev.ie_X = (WORD)dx;
    ev.ie_Y = (WORD)dy;
    in_send(&ev);
}

static void idrag(int x0, int y0, int x1, int y1, int steps, int add,
                  int hold)
{
    int i;

    if (!in_open(add)) {
        return;
    }
    if (steps < 1) {
        steps = 1;
    }
    printf("idrag: %s, qualifier before %04x\n",
           g_iadd ? "IND_ADDEVENT" : "IND_WRITEEVENT",
           (unsigned)PeekQualifier());
    in_at(x0, y0, 0);
    Delay(10);
    in_button(1);
    Delay(10);
    printf("idrag: qualifier after the press %04x\n",
           (unsigned)PeekQualifier());
    for (i = 1; i <= steps; i++) {
        in_at(x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * i / steps, 1);
        Delay(4);
    }
    Delay(20);
    printf("idrag: pointer %d %d%s\n",
           g_iscr ? (int)g_iscr->MouseX : -1, g_iscr ? (int)g_iscr->MouseY : -1,
           hold ? ", button still held (IUP releases it)" : "");
    if (!hold) {
        in_button(0);
        Delay(10);
    }
    in_close();
}

/*
 * Put a window back where a test wants it. The window is found by its
 * title under the Intuition lock; ChangeWindowBox() is called after the
 * lock is let go, as it must be: a window that closes in between is the
 * bench's own business.
 */
static void winbox(const char *title, int x, int y, int w, int h)
{
    struct Screen *scr;
    struct Window *win, *found = NULL;
    ULONG lock;

    IntuitionBase = (struct IntuitionBase *)
        OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    if (IntuitionBase == NULL) {
        return;
    }
    scr = LockPubScreen(NULL);
    if (scr != NULL) {
        lock = LockIBase(0);
        for (win = scr->FirstWindow; win != NULL; win = win->NextWindow) {
            if (win->Title != NULL && strcmp((const char *)win->Title, title) == 0) {
                found = win;
                break;
            }
        }
        UnlockIBase(lock);
        if (found != NULL) {
            ChangeWindowBox(found, (LONG)x, (LONG)y, (LONG)w, (LONG)h);
            Delay(10);
            printf("winbox: \"%s\" now %d,%d %dx%d\n", title,
                   (int)found->LeftEdge, (int)found->TopEdge,
                   (int)found->Width, (int)found->Height);
        } else {
            printf("winbox: no window \"%s\"\n", title);
        }
        UnlockPubScreen(NULL, scr);
    }
    CloseLibrary((struct Library *)IntuitionBase);
    IntuitionBase = NULL;
}

/* The screens, copied out under the lock and printed after it: no
 * output while Intuition is held, since a console needs Intuition. */
#define KS_MAX_SCREENS 16

struct KsScreen {
    ULONG addr;
    WORD w, h;
    UWORD flags;
    char title[64];
};

static void screens(void)
{
    static struct KsScreen list[KS_MAX_SCREENS];
    static struct KsScreen pub[KS_MAX_SCREENS];
    char def[MAXPUBSCREENNAME + 1];
    struct Screen *s;
    struct PubScreenNode *pn;
    struct List *pl;
    ULONG lock;
    int n = 0, np = 0, i;

    IntuitionBase = (struct IntuitionBase *)
        OpenLibrary((CONST_STRPTR)"intuition.library", 37);
    if (IntuitionBase == NULL) {
        return;
    }
    def[0] = '\0';
    GetDefaultPubScreen((UBYTE *)def);
    lock = LockIBase(0);
    for (s = IntuitionBase->FirstScreen; s != NULL && n < KS_MAX_SCREENS;
         s = s->NextScreen) {
        list[n].addr = (ULONG)s;
        list[n].w = s->Width;
        list[n].h = s->Height;
        list[n].flags = s->Flags;
        strncpy(list[n].title, s->Title ? (const char *)s->Title : "",
                sizeof(list[n].title) - 1);
        list[n].title[sizeof(list[n].title) - 1] = '\0';
        n++;
    }
    UnlockIBase(lock);
    pl = LockPubScreenList();
    for (pn = (struct PubScreenNode *)pl->lh_Head;
         pn->psn_Node.ln_Succ != NULL && np < KS_MAX_SCREENS;
         pn = (struct PubScreenNode *)pn->psn_Node.ln_Succ) {
        pub[np].addr = (ULONG)pn->psn_Screen;
        strncpy(pub[np].title, pn->psn_Node.ln_Name ? pn->psn_Node.ln_Name : "",
                sizeof(pub[np].title) - 1);
        pub[np].title[sizeof(pub[np].title) - 1] = '\0';
        np++;
    }
    UnlockPubScreenList();

    printf("default public screen: %s\n", def);
    for (i = 0; i < n; i++) {
        printf("screen %08lx %dx%d flags %04x%s \"%s\"\n",
               (unsigned long)list[i].addr, (int)list[i].w, (int)list[i].h,
               (unsigned)list[i].flags,
               (list[i].flags & SCREENTYPE) == WBENCHSCREEN ? " WORKBENCH" : "",
               list[i].title);
    }
    for (i = 0; i < np; i++) {
        printf("public \"%s\" = %08lx\n", pub[i].title,
               (unsigned long)pub[i].addr);
    }
    CloseLibrary((struct Library *)IntuitionBase);
    IntuitionBase = NULL;
}

/*
 * Can a window go past the edge of the screen? A small window that does
 * not take the focus opens near the right edge of the default public
 * screen and is asked to move 80 pixels beyond it. Intuition clamps a
 * move that the screen does not allow, and says nothing: the answer is
 * where the window ends up.
 */
static void offscreen(void)
{
    struct Screen *scr;
    struct Window *w;
    int sw, before, after;

    IntuitionBase = (struct IntuitionBase *)
        OpenLibrary((CONST_STRPTR)"intuition.library", 37);
    if (IntuitionBase == NULL) {
        return;
    }
    scr = LockPubScreen(NULL);
    if (scr == NULL) {
        printf("offscreen: no public screen\n");
        CloseLibrary((struct Library *)IntuitionBase);
        return;
    }
    sw = scr->Width;
    w = OpenWindowTags(NULL,
                       WA_PubScreen, (ULONG)scr,
                       WA_Left, sw - 200,
                       WA_Top, scr->Height / 2,
                       WA_Width, 160,
                       WA_Height, 60,
                       WA_Title, (ULONG)"keysend offscreen",
                       WA_DragBar, TRUE,
                       WA_Activate, FALSE,
                       WA_SimpleRefresh, TRUE,
                       WA_NoCareRefresh, TRUE,
                       TAG_DONE);
    UnlockPubScreen(NULL, scr);
    if (w == NULL) {
        printf("offscreen: the probe window would not open\n");
        CloseLibrary((struct Library *)IntuitionBase);
        return;
    }
    before = w->LeftEdge;
    MoveWindow(w, 120, 0);          /* its right edge 80 px past the screen's */
    Delay(25);                      /* MoveWindow is carried out by Intuition later */
    after = w->LeftEdge;
    printf("offscreen: screen %d wide, window %d wide, left %d -> %d "
           "(asked %d): %s\n", sw, (int)w->Width, before, after, before + 120,
           after + w->Width > sw ? "PAST THE EDGE, allowed" :
                                  "stopped at the edge");
    CloseWindow(w);
    CloseLibrary((struct Library *)IntuitionBase);
    IntuitionBase = NULL;
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
    } else if (same(argv[1], "SCREENS")) {
        screens();
    } else if (same(argv[1], "OFFSCREEN")) {
        offscreen();
    } else if (same(argv[1], "IDRAG") && argc >= 6) {
        int steps = 12, add = 0, hold = 0, k;

        for (k = 6; k < argc; k++) {
            if (same(argv[k], "ADD")) {
                add = 1;
            } else if (same(argv[k], "HOLD")) {
                hold = 1;
            } else if (argv[k][0] >= '0' && argv[k][0] <= '9') {
                steps = atoi(argv[k]);
            }
        }
        idrag(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]),
              steps, add, hold);
    } else if (same(argv[1], "IAT") && argc >= 4) {
        if (in_open(0)) {
            in_at(atoi(argv[2]), atoi(argv[3]), 1);
            Delay(5);
            in_close();
        }
    } else if (same(argv[1], "IPUSH") && argc >= 4) {
        int n = argc >= 5 ? atoi(argv[4]) : 1, k;

        if (in_open(0)) {
            for (k = 0; k < n; k++) {
                in_push(atoi(argv[2]), atoi(argv[3]));
                Delay(2);
            }
            in_close();
        }
    } else if (same(argv[1], "WINBOX") && argc >= 7) {
        winbox(argv[2], atoi(argv[3]), atoi(argv[4]), atoi(argv[5]),
               atoi(argv[6]));
    } else if (same(argv[1], "IUP")) {
        if (in_open(0)) {
            in_button(0);
            Delay(10);
            in_close();
        }
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
