/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * icontrol_offscreen.c - see icontrol_offscreen.h.
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>

#include "icontrol.h"
#include "icontrol_offscreen.h"

/* The whole file, -1 when it is not there, or -2 for a read error
 * or a file longer than any IControl file. */
static int eso_read(const char *name, unsigned char *buf)
{
    BPTR fh = Open((CONST_STRPTR)name, MODE_OLDFILE);
    LONG n;

    if (fh == 0) {
        LONG err = IoErr();

        return err == ERROR_OBJECT_NOT_FOUND || err == ERROR_DIR_NOT_FOUND ?
               -1 : -2;
    }
    n = Read(fh, buf, ES_ICTL_FILE_MAX);
    if (n == ES_ICTL_FILE_MAX) {
        UBYTE more;

        if (Read(fh, &more, 1) != 0) {
            n = -2;
        }
    }
    Close(fh);
    return n < 0 ? -2 : (int)n;
}

static int eso_write(const char *name, const unsigned char *buf, int len)
{
    BPTR fh = Open((CONST_STRPTR)name, MODE_NEWFILE);
    LONG n;

    if (fh == 0) {
        return 0;
    }
    n = Write(fh, (APTR)buf, len);
    Close(fh);
    return n == len;
}

int eso_query(int archived)
{
    static unsigned char buf[ES_ICTL_FILE_MAX];
    int len = -1;

    if (!archived) {
        len = eso_read(ESO_ENV, buf);
    }
    if (len == -1) {
        len = eso_read(ESO_ENVARC, buf);
    }
    if (len == -1) {
        return 0;                  /* no file: the system's default */
    }
    if (len < 0) {
        return -1;
    }
    return es_ictl_offscreen(buf, len);
}

/* One file: what is there, or what the other place has, or the
 * defaults, with the bit as asked. */
static int eso_set_one(const char *name, const char *other, int on)
{
    static unsigned char buf[ES_ICTL_FILE_MAX];
    int len = eso_read(name, buf);

    if (len == -1) {
        len = eso_read(other, buf);
    }
    if (len == -1) {
        len = es_ictl_default(buf, on);
    }
    if (len < 0 || es_ictl_set_offscreen(buf, len, on) < 0) {
        return 0;                  /* not a file we understand: hands off */
    }
    return eso_write(name, buf, len);
}

int eso_set(int on, int live, int archived)
{
    int ok = 1;

    /* the archive first: should the live one fail, what survives the
     * next start is already what was asked for */
    if (archived && !eso_set_one(ESO_ENVARC, ESO_ENV, on)) {
        ok = 0;
    }
    if (live && !eso_set_one(ESO_ENV, ESO_ENVARC, on)) {
        ok = 0;
    }
    return ok;
}
