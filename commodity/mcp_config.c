/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * mcp_config.c - see mcp_config.h.
 */
#include <string.h>

#include "mcp_config.h"

static const char *const esm_on[2] = {
    "[MOVEWINDOW=ON]", "[SIZEWINDOW=ON]"
};
static const char *const esm_off[2] = {
    "[MOVEWINDOW=OFF]", "[SIZEWINDOW=OFF]"
};

int esm_is_config(const char *text, long len)
{
    return text != 0 && len >= 10 && strncmp(text, "MCP-CONFIG", 10) == 0;
}

/* 'line' where it is a whole line of the text, or 0 */
static char *esm_line(char *text, const char *line)
{
    size_t n = strlen(line);
    char *p = text;

    while ((p = strstr(p, line)) != 0) {
        if ((p == text || p[-1] == '\n') &&
            (p[n] == '\n' || p[n] == '\r' || p[n] == '\0')) {
            return p;
        }
        p += n;
    }
    return 0;
}

int esm_turn(char *text, long *len, int which, int off)
{
    int i, done = 0;

    for (i = 0; i < 2; i++) {
        const char *from = off ? esm_on[i] : esm_off[i];
        const char *to = off ? esm_off[i] : esm_on[i];
        long at, n_from, n_to;
        char *p;

        if (!(which & (1 << i)) || (p = esm_line(text, from)) == 0) {
            continue;
        }
        n_from = (long)strlen(from);
        n_to = (long)strlen(to);
        at = (long)(p - text);
        /* the tail, its NUL included, moves by the difference */
        memmove(text + at + n_to, text + at + n_from,
                (size_t)(*len - at - n_from + 1));
        memcpy(text + at, to, (size_t)n_to);
        *len += n_to - n_from;
        done |= 1 << i;
    }
    return done;
}
