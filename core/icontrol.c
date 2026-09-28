/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * icontrol.c - see icontrol.h. An IFF FORM PREF: a PRHD chunk, then
 * ICTL holding struct IControlPrefs, big-endian as everything Amiga.
 * ic_Flags comes after four reserved longs, the timeout and the meta
 * drag qualifier: twenty bytes into the chunk.
 */

#include "icontrol.h"

#define ES_ICTL_FLAGS_IN_CHUNK 20
#define ES_ICTL_BIT_BYTE       2      /* bit 15 is in the third byte ... */
#define ES_ICTL_BIT_MASK       0x80   /* ... of the four, its top bit    */

static unsigned long es_be32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) |
           ((unsigned long)p[2] << 8) | (unsigned long)p[3];
}

static int es_is(const unsigned char *p, const char *id)
{
    return p[0] == (unsigned char)id[0] && p[1] == (unsigned char)id[1] &&
           p[2] == (unsigned char)id[2] && p[3] == (unsigned char)id[3];
}

int es_ictl_flags_at(const unsigned char *buf, int len)
{
    int pos = 12;

    if (buf == 0 || len < 12 || !es_is(buf, "FORM") ||
        !es_is(buf + 8, "PREF")) {
        return -1;
    }
    while (pos + 8 <= len) {
        unsigned long size = es_be32(buf + pos + 4);

        if (size > (unsigned long)(len - pos - 8)) {
            return -1;                     /* a chunk longer than the file */
        }
        if (es_is(buf + pos, "ICTL")) {
            if (size < ES_ICTL_FLAGS_IN_CHUNK + 4) {
                return -1;
            }
            return pos + 8 + ES_ICTL_FLAGS_IN_CHUNK;
        }
        pos += 8 + (int)size + (int)(size & 1);   /* chunks are padded even */
    }
    return -1;
}

int es_ictl_offscreen(const unsigned char *buf, int len)
{
    int at = es_ictl_flags_at(buf, len);

    if (at < 0) {
        return -1;
    }
    return (buf[at + ES_ICTL_BIT_BYTE] & ES_ICTL_BIT_MASK) != 0;
}

int es_ictl_set_offscreen(unsigned char *buf, int len, int on)
{
    int at = es_ictl_flags_at(buf, len);
    unsigned char was;

    if (at < 0) {
        return -1;
    }
    was = buf[at + ES_ICTL_BIT_BYTE];
    if (on) {
        buf[at + ES_ICTL_BIT_BYTE] = (unsigned char)(was | ES_ICTL_BIT_MASK);
    } else {
        buf[at + ES_ICTL_BIT_BYTE] =
            (unsigned char)(was & (unsigned char)~ES_ICTL_BIT_MASK);
    }
    return buf[at + ES_ICTL_BIT_BYTE] != was;
}

int es_ictl_default(unsigned char *buf, int on)
{
    static const unsigned char file[ES_ICTL_DEFAULT_LEN] = {
        'F', 'O', 'R', 'M', 0x00, 0x00, 0x00, 0x40,
        'P', 'R', 'E', 'F',
        'P', 'R', 'H', 'D', 0x00, 0x00, 0x00, 0x06,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        'I', 'C', 'T', 'L', 0x00, 0x00, 0x00, 0x26,
        /* ic_Reserved[4] */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x32,                 /* ic_TimeOut: 50                  */
        0x00, 0x40,                 /* ic_MetaDrag: left Amiga         */
        0x80, 0x00, 0x00, 0x1E,     /* ic_Flags: versioned, mode        */
                                    /* promotion, menu snap, string     */
                                    /* filter, avoid flicker            */
        'N', 'M', 'V', 'B',         /* the four command keys            */
        0x00, 0x02,                 /* ic_Version                       */
        0x00, 0x00,                 /* ic_VersionMagic                  */
        0x00, 0x00,                 /* hover, pad                       */
        0x00, 0x00, 0x00, 0x00      /* ic_GUIGeometry                   */
    };
    int i;

    for (i = 0; i < ES_ICTL_DEFAULT_LEN; i++) {
        buf[i] = file[i];
    }
    es_ictl_set_offscreen(buf, ES_ICTL_DEFAULT_LEN, on);
    return ES_ICTL_DEFAULT_LEN;
}
