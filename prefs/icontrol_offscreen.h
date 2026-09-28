/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * icontrol_offscreen.h - AmigaOS's own setting "windows can move
 * off-screen", read and written where AmigaOS keeps it: the IControl
 * preferences file. What the bit is and where it sits is
 * core/icontrol.c's; this is the dos.library around it.
 *
 * Only for the classic systems, and only where the setting exists:
 * the caller checks for intuition.library 47 before offering it.
 * Writing the live file makes IPrefs hand it to Intuition, which then
 * asks to reset the Workbench screen; writing only the archived one
 * changes nothing until the system starts again.
 */

#ifndef EDGESNAP_ICONTROL_OFFSCREEN_H
#define EDGESNAP_ICONTROL_OFFSCREEN_H

#define ESO_ENV     "ENV:Sys/icontrol.prefs"
#define ESO_ENVARC  "ENVARC:Sys/icontrol.prefs"

/*
 * May windows move off-screen? 1 yes, 0 no, -1 when the file is there
 * and is not one this code understands, in which case it is never
 * written either. A machine with no file has the system's default,
 * which is no. `archived` asks what the next start will use, ENVARC:,
 * instead of what is in effect now.
 */
int eso_query(int archived);

/*
 * Set it. Every other byte of the user's file is kept; a machine with
 * no file gets the one the IControl editor writes from the defaults.
 * `live` writes ENV:, `archived` ENVARC:. Returns 1 when everything
 * asked for was written.
 */
int eso_set(int on, int live, int archived);

#endif /* EDGESNAP_ICONTROL_OFFSCREEN_H */
