/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * edgesnap_library.c - the classic skeleton of edgesnap.library: 68k,
 * AmigaOS 2.04 and later, the systems the library was shaped after.
 *
 * Same body, the original clothes. A Resident tag that exec finds in
 * the file, an auto-init table, and the negative-offset jump table
 * with every vector entered through the 68k ABI: here the compiler
 * itself picks each argument out of the register the .fd names, so
 * the gates are plain C functions with register parameters. The
 * register assignment IS the ABI: it matches library/morphos/
 * edgesnap_lib.fd and must never be reordered once released.
 *
 * As on the other systems the library links without a C runtime, so
 * it provides the memcpy/memset the compiler emits for structure
 * assignment, and this file must come first on the link line: the
 * first code in the binary is what a Shell runs when someone types the
 * library's name, and that must return quietly.
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/resident.h>
#include <exec/nodes.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <utility/tagitem.h>

#include <proto/exec.h>

#include "edgesnap.h"
#include "edgesnap_body.h"

/* Built with -fno-toplevel-reorder, so this stays the first code. */
LONG ES_LibStart(void);
LONG ES_LibStart(void)
{
    return -1;
}

#define ES_LIB_NAME     "edgesnap.library"
#define ES_LIB_VERSION  2
#define ES_LIB_REVISION 28
#define ES_LIB_IDSTRING "edgesnap.library 2.28 (02.11.2026) Michele Dipace\r\n"

/* An argument in the register the ABI names. */
#define ES_REG(reg, decl) decl __asm(#reg)

/*
 * Defined here, with their value: a bare declaration is only a common
 * symbol, and -lc then brought in libnix's own DOSBase, which starts
 * at -1 (seen on 2026-10-20). LibOpen opens a base that is still NULL,
 * so that -1 was never replaced, and the expunge closed it.
 */
struct ExecBase *SysBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct DosLibrary *DOSBase = NULL;   /* Delay, for the glide's tick */

struct EdgeSnapBase {
    struct Library lib;
    BPTR segList;
};

/* The compiler emits these for structure assignment; a shared library
 * must not depend on a C runtime, so they live here. Built with
 * -fno-tree-loop-distribute-patterns so GCC cannot rewrite the loops
 * into calls to themselves. */
void *memcpy(void *dst, const void *src, unsigned long n);
void *memset(void *dst, int c, unsigned long n);

void *memcpy(void *dst, const void *src, unsigned long n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    while (n-- > 0) {
        *d++ = *s++;
    }
    return dst;
}

void *memset(void *dst, int c, unsigned long n)
{
    unsigned char *d = (unsigned char *)dst;

    while (n-- > 0) {
        *d++ = (unsigned char)c;
    }
    return dst;
}

/* ------------------------------------------------------ library node */

/* RTF_AUTOINIT: exec has made the node from the init table and calls
 * this with the base in d0, the segment list in a0, itself in a6. */
static struct Library *LibInit(ES_REG(d0, struct EdgeSnapBase *esb),
                               ES_REG(a0, BPTR seglist),
                               ES_REG(a6, struct ExecBase *sysbase))
{
    SysBase = sysbase;
    esb->lib.lib_Node.ln_Type = NT_LIBRARY;
    esb->lib.lib_Node.ln_Pri = 0;
    esb->lib.lib_Node.ln_Name = (char *)ES_LIB_NAME;
    esb->lib.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    esb->lib.lib_Version = ES_LIB_VERSION;
    esb->lib.lib_Revision = ES_LIB_REVISION;
    esb->lib.lib_IdString = (char *)ES_LIB_IDSTRING;
    esb->segList = seglist;
    return &esb->lib;
}

static struct Library *LibOpen(ES_REG(d0, ULONG version),
                               ES_REG(a6, struct EdgeSnapBase *esb))
{
    (void)version;
    if (esb->lib.lib_OpenCnt == 0) {
        /* First client: what the body needs, then the body. */
        if (IntuitionBase == NULL) {
            IntuitionBase = (struct IntuitionBase *)
                OpenLibrary((CONST_STRPTR)"intuition.library", 36);
        }
        if (DOSBase == NULL) {
            DOSBase = (struct DosLibrary *)
                OpenLibrary((CONST_STRPTR)"dos.library", 36);
        }
        if (IntuitionBase == NULL || DOSBase == NULL || !esb_init()) {
            if (DOSBase != NULL) {
                CloseLibrary((struct Library *)DOSBase);
                DOSBase = NULL;
            }
            if (IntuitionBase != NULL) {
                CloseLibrary((struct Library *)IntuitionBase);
                IntuitionBase = NULL;
            }
            return NULL;
        }
    }
    esb->lib.lib_OpenCnt++;
    esb->lib.lib_Flags &= ~LIBF_DELEXP;
    return &esb->lib;
}

static BPTR LibExpungeBase(struct EdgeSnapBase *esb)
{
    BPTR seglist;

    if (esb->lib.lib_OpenCnt != 0) {
        esb->lib.lib_Flags |= LIBF_DELEXP;
        return (BPTR)0;
    }
    esb_cleanup();
    if (IntuitionBase != NULL) {
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = NULL;
    }
    if (DOSBase != NULL) {
        CloseLibrary((struct Library *)DOSBase);
        DOSBase = NULL;
    }
    seglist = esb->segList;
    Remove(&esb->lib.lib_Node);
    FreeMem((APTR)((ULONG)esb - esb->lib.lib_NegSize),
            esb->lib.lib_NegSize + esb->lib.lib_PosSize);
    return seglist;
}

static BPTR LibClose(ES_REG(a6, struct EdgeSnapBase *esb))
{
    if (esb->lib.lib_OpenCnt > 0) {
        esb->lib.lib_OpenCnt--;
    }
    if (esb->lib.lib_OpenCnt == 0 && (esb->lib.lib_Flags & LIBF_DELEXP)) {
        return LibExpungeBase(esb);
    }
    return (BPTR)0;
}

static BPTR LibExpunge(ES_REG(a6, struct EdgeSnapBase *esb))
{
    return LibExpungeBase(esb);
}

static ULONG LibReserved(void)
{
    return 0;
}

/* ------------------------------------------- API gates (68k ABI in) */

/*
 * Register assignment - this is the ABI, mirrored in edgesnap_lib.fd:
 * the compiler reads each argument from the register named here.
 */

static ULONG G_QueryCapabilities(void)
{
    return esb_query_capabilities();
}

static LONG G_SnapWindow(ES_REG(a0, struct Window *win),
                         ES_REG(d0, ULONG zone))
{
    return esb_snap_window(win, zone);
}

static LONG G_UnsnapWindow(ES_REG(a0, struct Window *win))
{
    return esb_unsnap_window(win);
}

static LONG G_QueryWindow(ES_REG(a0, struct Window *win),
                          ES_REG(a1, ULONG *zone_out))
{
    return esb_query_window(win, zone_out);
}

static LONG G_ExcludeWindow(ES_REG(a0, struct Window *win),
                            ES_REG(d0, ULONG exclude))
{
    return esb_exclude_window(win, (BOOL)exclude);
}

static LONG G_SetOptionsA(ES_REG(a0, const struct TagItem *tags))
{
    return esb_set_options(tags);
}

static LONG G_Enable(ES_REG(d0, ULONG on))
{
    return esb_enable((BOOL)on);
}

static void G_FeedInput(ES_REG(d0, ULONG presses),
                        ES_REG(d1, ULONG motions),
                        ES_REG(d2, ULONG releases),
                        ES_REG(d3, ULONG qualifiers),
                        ES_REG(a0, struct ESnapReport *report))
{
    esb_input((int)presses, (int)motions, (int)releases, qualifiers, report);
}

static void G_ResetInput(ES_REG(a0, struct ESnapReport *report))
{
    esb_input_reset(report);
}

static LONG G_IgnoreWindows(ES_REG(a0, struct Window **windows),
                            ES_REG(d0, ULONG count))
{
    esb_ignore_windows(windows, (int)count);
    return ES_OK;
}

static LONG G_QueryScreenArea(ES_REG(a0, struct Screen *screen),
                              ES_REG(a1, struct ESnapArea *area))
{
    return esb_query_screen_area(screen, area);
}

static LONG G_QueryDivider(ES_REG(d0, ULONG thickness),
                           ES_REG(a0, struct ESnapDivider *divider))
{
    return esb_query_divider(thickness, divider);
}

static LONG G_MoveDivider(ES_REG(d0, LONG position))
{
    return esb_move_divider(position);
}

static LONG G_QueryDividerAt(ES_REG(d0, ULONG thickness),
                             ES_REG(d1, LONG x),
                             ES_REG(d2, LONG y),
                             ES_REG(a0, struct ESnapDivider *divider))
{
    return esb_query_divider_at(thickness, x, y, divider);
}

static LONG G_MoveDividerAt(ES_REG(a0, LONG vertical),
                            ES_REG(a1, LONG line),
                            ES_REG(d0, LONG position))
{
    return esb_move_divider_at(vertical, line, position);
}

/* --- appended for 2.5 --- */

static LONG G_QueryWindows(ES_REG(a0, struct Screen *screen),
                           ES_REG(a1, struct ESnapWindowInfo *buf),
                           ES_REG(d0, ULONG count),
                           ES_REG(a2, ULONG *needed))
{
    return esb_query_windows(screen, buf, count, needed);
}

static ULONG G_QueryGeneration(ES_REG(a0, struct Screen *screen))
{
    return esb_query_generation(screen);
}

static LONG G_PlaceWindow(ES_REG(a0, struct Window *win),
                          ES_REG(a1, const struct ESnapRect *rect),
                          ES_REG(d0, ULONG flags))
{
    return esb_place_window(win, rect, flags);
}

static LONG G_PlaceWindowsA(ES_REG(a0, struct ESnapPlacement *list),
                            ES_REG(d0, ULONG count),
                            ES_REG(d1, ULONG flags))
{
    return esb_place_windows(list, count, flags);
}

/* --- appended for 2.6 --- */

static LONG G_QueryWindowSerial(ES_REG(a0, struct Window *win),
                                ES_REG(a1, ULONG *serial))
{
    return esb_query_window_serial(win, serial);
}

static LONG G_FindWindow(ES_REG(d0, ULONG serial),
                         ES_REG(a0, struct Window **window))
{
    return esb_find_window(serial, window);
}

/* --- appended for 2.7 --- */

static void G_FeedMotion(ES_REG(d0, LONG dx), ES_REG(d1, LONG dy))
{
    esb_feed_motion(dx, dy);
}

/* --- appended for 2.10: client roles --- */

static LONG G_RegisterClient(ES_REG(a0, const char *name),
                             ES_REG(d0, ULONG wants),
                             ES_REG(a1, ULONG *client_out))
{
    return esb_register_client(name, wants, client_out);
}

static LONG G_UnregisterClient(ES_REG(d0, ULONG client))
{
    return esb_unregister_client(client);
}

/* --- appended for 2.11: work areas --- */

static LONG G_QueryWorkAreas(ES_REG(a0, struct Screen *screen),
                             ES_REG(a1, struct ESnapWorkArea *buf),
                             ES_REG(d0, ULONG count),
                             ES_REG(a2, ULONG *needed))
{
    return esb_query_work_areas(screen, buf, count, needed);
}

/* --- appended for 2.12: layout groups --- */

static LONG G_CreateGroup(ES_REG(d0, ULONG area),
                          ES_REG(a0, const char *name),
                          ES_REG(d1, ULONG flags),
                          ES_REG(a1, ULONG *group_out))
{
    return esb_create_group(area, name, flags, group_out);
}

static LONG G_DeleteGroup(ES_REG(d0, ULONG group))
{
    return esb_delete_group(group);
}

static LONG G_GroupAddWindow(ES_REG(d0, ULONG group),
                             ES_REG(a0, struct Window *win))
{
    return esb_group_add_window(group, win);
}

static LONG G_GroupRemoveWindow(ES_REG(d0, ULONG group),
                                ES_REG(a0, struct Window *win))
{
    return esb_group_remove_window(group, win);
}

static LONG G_QueryGroupOf(ES_REG(a0, struct Window *win),
                           ES_REG(a1, ULONG *group_out))
{
    return esb_query_group_of(win, group_out);
}

/* --- appended for 2.20 (2.21 here): gaps and margins per group --- */
static LONG G_SetGroupOptionsA(ES_REG(d0, ULONG group),
                               ES_REG(a0, const struct TagItem *tags))
{
    return esb_set_group_options(group, tags);
}

/* --- appended for 2.24 (2.26 here): divider tokens per group --- */
static LONG G_QueryDividers(ES_REG(d0, ULONG group),
                            ES_REG(a0, struct ESnapDividerInfo *buf),
                            ES_REG(d1, ULONG count),
                            ES_REG(a1, ULONG *needed))
{
    return esb_query_dividers(group, buf, count, needed);
}

static LONG G_MoveDividerT(ES_REG(d0, ULONG token),
                           ES_REG(d1, LONG position),
                           ES_REG(d2, ULONG flags),
                           ES_REG(a0, LONG *applied))
{
    return esb_move_divider_t(token, position, flags, applied);
}

static LONG G_QueryDividerWindows(ES_REG(d0, ULONG token),
                                  ES_REG(a0, ULONG *serials),
                                  ES_REG(d1, ULONG count),
                                  ES_REG(a1, ULONG *needed))
{
    return esb_query_divider_windows(token, serials, count, needed);
}

/* Vector order is the ABI. Append only, never reorder, never remove. */
static const APTR FuncTable[] = {
    (APTR)LibOpen,
    (APTR)LibClose,
    (APTR)LibExpunge,
    (APTR)LibReserved,
    (APTR)G_QueryCapabilities,
    (APTR)G_SnapWindow,
    (APTR)G_UnsnapWindow,
    (APTR)G_QueryWindow,
    (APTR)G_ExcludeWindow,
    (APTR)G_SetOptionsA,
    (APTR)G_Enable,
    (APTR)G_FeedInput,
    (APTR)G_ResetInput,
    (APTR)G_IgnoreWindows,
    (APTR)G_QueryScreenArea,
    (APTR)G_QueryDivider,
    (APTR)G_MoveDivider,
    (APTR)G_QueryDividerAt,
    (APTR)G_MoveDividerAt,
    (APTR)G_QueryWindows,
    (APTR)G_QueryGeneration,
    (APTR)G_PlaceWindow,
    (APTR)G_PlaceWindowsA,
    (APTR)G_QueryWindowSerial,
    (APTR)G_FindWindow,
    (APTR)G_FeedMotion,
    (APTR)G_RegisterClient,
    (APTR)G_UnregisterClient,
    (APTR)G_QueryWorkAreas,
    (APTR)G_CreateGroup,
    (APTR)G_DeleteGroup,
    (APTR)G_GroupAddWindow,
    (APTR)G_GroupRemoveWindow,
    (APTR)G_QueryGroupOf,
    (APTR)G_SetGroupOptionsA,
    (APTR)G_QueryDividers,
    (APTR)G_MoveDividerT,
    (APTR)G_QueryDividerWindows,
    (APTR)-1
};

static const APTR InitTable[4] = {
    (APTR)sizeof(struct EdgeSnapBase),
    (APTR)FuncTable,
    NULL,
    (APTR)LibInit
};

extern const struct Resident RomTag;

static const char LibName[] = ES_LIB_NAME;
static const char LibIdString[] = ES_LIB_IDSTRING;

const struct Resident RomTag __attribute__((used)) = {
    RTC_MATCHWORD,
    (struct Resident *)&RomTag,
    (APTR)(&RomTag + 1),
    RTF_AUTOINIT,
    ES_LIB_VERSION,
    NT_LIBRARY,
    0,
    (char *)LibName,
    (char *)LibIdString,
    (APTR)InitTable
};

static const char verstag[] __attribute__((used)) =
    "$VER: " ES_LIB_IDSTRING;
