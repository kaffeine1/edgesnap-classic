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
 *   keysend MOVE dx dy                     the mouse, relative
 *   keysend BUTTON DOWN | UP               the left button
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
#include <clib/alib_protos.h>   /* InvertString, FreeIEvents: amiga.lib */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Library *CxBase;

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

static void mouse(UWORD code, WORD dx, WORD dy)
{
    struct InputEvent ev;

    memset(&ev, 0, sizeof(ev));
    ev.ie_Class = IECLASS_RAWMOUSE;
    ev.ie_Code = code;
    ev.ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE;
    ev.ie_X = dx;
    ev.ie_Y = dy;
    AddIEvents(&ev);
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
    } else if (same(argv[1], "MOVE") && argc >= 4) {
        mouse(IECODE_NOBUTTON, (WORD)atoi(argv[2]), (WORD)atoi(argv[3]));
    } else if (same(argv[1], "BUTTON") && argc >= 3) {
        if (same(argv[2], "DOWN")) {
            mouse(IECODE_LBUTTON, 0, 0);
        } else {
            mouse(IECODE_LBUTTON | IECODE_UP_PREFIX, 0, 0);
        }
    } else {
        printf("keysend: what?\n");
        rc = 5;
    }
    CloseLibrary(CxBase);
    return rc;
}
