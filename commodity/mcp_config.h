/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * mcp_config.h - the two lines of MCP's configuration that EdgeSnap
 * turns off while it runs: MOVEWINDOW and SIZEWINDOW, MCP's own moving
 * and sizing of windows in real time. Pure C over the file's text, so
 * the host tests hold it to every byte before an Amiga's ENV: sees it;
 * the commodity only reads and writes the file (spike_mcp_turn).
 */
#ifndef EDGESNAP_MCP_CONFIG_H
#define EDGESNAP_MCP_CONFIG_H

#define ESM_MOVEWINDOW 1
#define ESM_SIZEWINDOW 2
#define ESM_BOTH       3

/* Room a turn may need beyond the text's length: one byte per line. */
#define ESM_SLACK      2

/* 1 when 'text' starts as MCP's configuration does. */
int esm_is_config(const char *text, long len);

/*
 * Turn the lines named by 'which' to OFF (off != 0) or back to ON, in
 * place. 'text' holds *len bytes and a NUL after them, in a buffer
 * with ESM_SLACK bytes more. A line counts only where it is a whole
 * line, "[MOVEWINDOW=ON]" and nothing else. Returns the lines it
 * turned, and updates *len.
 */
int esm_turn(char *text, long *len, int which, int off);

#endif /* EDGESNAP_MCP_CONFIG_H */
