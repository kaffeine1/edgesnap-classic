/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * icontrol.h - the one bit of AmigaOS's IControl preferences that
 * EdgeSnap cares about: whether a window may be dragged past the edge
 * of the screen ("Can Move Off-screen" in the IControl editor of
 * AmigaOS 3.2, ICF_OFFSCRNWIN in prefs/icontrol.h).
 *
 * Why it matters here: with it off, a window dragged against an edge
 * stops there and keeps the pointer where it holds the title bar, so
 * the pointer never reaches the edge and a drag never snaps.
 *
 * Pure C89 over a file held in memory, so the host tests can hold it
 * to what the real files look like; the reading and writing are the
 * preferences program's (prefs/icontrol_offscreen.c). Only this bit is
 * ever touched: every other byte of the user's file goes back as it
 * came.
 */

#ifndef EDGESNAP_ICONTROL_H
#define EDGESNAP_ICONTROL_H

/* Longer than any icontrol.prefs: 72 bytes as AmigaOS 3.2 writes it,
 * more with the exceptions chunk some setups add. */
#define ES_ICTL_FILE_MAX    1024

/* The length of the file es_ictl_default() makes. */
#define ES_ICTL_DEFAULT_LEN 72

/*
 * Where the four bytes of ic_Flags are in the file, or -1 when the
 * buffer is not an IControl preferences file.
 */
int es_ictl_flags_at(const unsigned char *buf, int len);

/* 1 when windows may leave the screen, 0 when not, -1 when the buffer
 * is not an IControl preferences file. */
int es_ictl_offscreen(const unsigned char *buf, int len);

/*
 * Set or clear the bit in place. 1 when the buffer changed, 0 when it
 * already said so, -1 when it is not an IControl preferences file, in
 * which case nothing is touched.
 */
int es_ictl_set_offscreen(unsigned char *buf, int len, int on);

/*
 * The file the IControl editor of AmigaOS 3.2 writes from the system's
 * own defaults, with the bit as asked: for a machine whose user never
 * saved IControl, which has no file at all. Byte for byte what the
 * editor wrote on a fresh 3.2.3 when only that box was changed
 * (2026-09-28). Needs ES_ICTL_DEFAULT_LEN bytes, returns that length.
 */
int es_ictl_default(unsigned char *buf, int on);

#endif /* EDGESNAP_ICONTROL_H */
