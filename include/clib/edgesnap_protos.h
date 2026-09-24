/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * clib/edgesnap_protos.h - the prototypes of edgesnap.library, one per
 * line, in the shape the classic tools expect: fd2sfd pairs them with
 * library/morphos/edgesnap_lib.fd (the ABI) to make the sfd from which
 * sfdc writes the inline stubs and proto header for the 68k client.
 * The prototypes are the ones in include/edgesnap.h, where each call
 * is documented; keep the two in step (the build checks that they are).
 */

#ifndef CLIB_EDGESNAP_PROTOS_H
#define CLIB_EDGESNAP_PROTOS_H

#include <exec/types.h>
#include <intuition/intuition.h>
#include <utility/tagitem.h>
#include "edgesnap_types.h"

ULONG ESnap_QueryCapabilities(void);
LONG ESnap_SnapWindow(struct Window *win, ULONG zone);
LONG ESnap_UnsnapWindow(struct Window *win);
LONG ESnap_QueryWindow(struct Window *win, ULONG *zone_out);
LONG ESnap_ExcludeWindow(struct Window *win, BOOL exclude);
LONG ESnap_SetOptionsA(const struct TagItem *tags);
LONG ESnap_Enable(BOOL on);
void ESnap_FeedInput(ULONG presses, ULONG motions, ULONG releases, ULONG qualifiers, struct ESnapReport *report);
void ESnap_ResetInput(struct ESnapReport *report);
LONG ESnap_IgnoreWindows(struct Window **windows, ULONG count);
LONG ESnap_QueryScreenArea(struct Screen *screen, struct ESnapArea *area);
LONG ESnap_QueryDivider(ULONG thickness, struct ESnapDivider *divider);
LONG ESnap_MoveDivider(LONG position);
LONG ESnap_QueryDividerAt(ULONG thickness, LONG x, LONG y, struct ESnapDivider *divider);
LONG ESnap_MoveDividerAt(LONG vertical, LONG line, LONG position);
LONG ESnap_QueryWindows(struct Screen *screen, struct ESnapWindowInfo *buf, ULONG count, ULONG *needed);
ULONG ESnap_QueryGeneration(struct Screen *screen);
LONG ESnap_PlaceWindow(struct Window *win, const struct ESnapRect *rect, ULONG flags);
LONG ESnap_PlaceWindowsA(struct ESnapPlacement *list, ULONG count, ULONG flags);
LONG ESnap_QueryWindowSerial(struct Window *win, ULONG *serial);
LONG ESnap_FindWindow(ULONG serial, struct Window **window);
void ESnap_FeedMotion(LONG dx, LONG dy);
LONG ESnap_RegisterClient(const char *name, ULONG wants, ULONG *client_out);
LONG ESnap_UnregisterClient(ULONG client);
LONG ESnap_QueryWorkAreas(struct Screen *screen, struct ESnapWorkArea *buf, ULONG count, ULONG *needed);
LONG ESnap_CreateGroup(ULONG workArea, const char *name, ULONG flags, ULONG *group_out);
LONG ESnap_DeleteGroup(ULONG group);
LONG ESnap_GroupAddWindow(ULONG group, struct Window *win);
LONG ESnap_GroupRemoveWindow(ULONG group, struct Window *win);
LONG ESnap_QueryGroupOf(struct Window *win, ULONG *group_out);

#endif /* CLIB_EDGESNAP_PROTOS_H */
