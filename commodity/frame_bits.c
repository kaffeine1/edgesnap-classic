/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * frame_bits.c - see frame_bits.h.
 */

#include "frame_bits.h"

unsigned long esf_planar_bytes(int x, int w, int h, int depth)
{
    long first, last;

    if (w <= 0 || h <= 0 || depth <= 0) {
        return 0;
    }
    first = (long)x >> 3;
    last = (long)(x + w - 1) >> 3;
    return (unsigned long)(last - first + 1) * (unsigned long)h *
           (unsigned long)depth;
}

unsigned long esf_chunky_bytes(int w, int h, int bpp)
{
    if (w <= 0 || h <= 0 || bpp <= 0) {
        return 0;
    }
    return (unsigned long)w * (unsigned long)h * (unsigned long)bpp;
}

/* The bits of byte `b` that belong to columns x .. x + w - 1. */
static unsigned char esf_mask(long b, long first, long last, int x, int w)
{
    unsigned int m = 0xFFu;

    if (b == first) {
        m &= 0xFFu >> (x & 7);
    }
    if (b == last) {
        m &= (0xFFu << (7 - ((x + w - 1) & 7))) & 0xFFu;
    }
    return (unsigned char)m;
}

void esf_planar_show(const ESFPlanes *bm, int x, int y, int w, int h,
                     unsigned int pen, unsigned char *keep)
{
    long first = (long)x >> 3, last = (long)(x + w - 1) >> 3, b;
    int k, row;

    if (w <= 0 || h <= 0) {
        return;
    }
    for (k = 0; k < bm->depth; k++) {
        unsigned char fill = (unsigned char)(((pen >> k) & 1u) ? 0xFF : 0x00);

        for (row = 0; row < h; row++) {
            unsigned char *p = bm->plane[k] + (long)(y + row) * bm->stride +
                               first;

            for (b = first; b <= last; b++, p++) {
                unsigned char m = esf_mask(b, first, last, x, w);
                unsigned char was = *p;

                *keep++ = (unsigned char)((was ^ fill) & m);
                *p = (unsigned char)((was & (unsigned char)~m) | (fill & m));
            }
        }
    }
}

void esf_planar_drop(const ESFPlanes *bm, int x, int y, int w, int h,
                     const unsigned char *keep)
{
    long first = (long)x >> 3, last = (long)(x + w - 1) >> 3, b;
    int k, row;

    if (w <= 0 || h <= 0) {
        return;
    }
    for (k = 0; k < bm->depth; k++) {
        for (row = 0; row < h; row++) {
            unsigned char *p = bm->plane[k] + (long)(y + row) * bm->stride +
                               first;

            for (b = first; b <= last; b++, p++) {
                *p = (unsigned char)(*p ^ *keep++);
            }
        }
    }
}

void esf_chunky_show(const ESFChunky *bm, int x, int y, int w, int h,
                     const unsigned char *accent, unsigned char *keep)
{
    long n = (long)w * bm->bpp, i;
    int row;

    if (w <= 0 || h <= 0) {
        return;
    }
    for (row = 0; row < h; row++) {
        unsigned char *p = bm->base + (long)(y + row) * bm->stride +
                           (long)x * bm->bpp;
        int c = 0;

        for (i = 0; i < n; i++) {
            *keep++ = (unsigned char)(p[i] ^ accent[c]);
            p[i] = accent[c];
            if (++c == bm->bpp) {
                c = 0;
            }
        }
    }
}

void esf_chunky_drop(const ESFChunky *bm, int x, int y, int w, int h,
                     const unsigned char *keep)
{
    long n = (long)w * bm->bpp, i;
    int row;

    if (w <= 0 || h <= 0) {
        return;
    }
    for (row = 0; row < h; row++) {
        unsigned char *p = bm->base + (long)(y + row) * bm->stride +
                           (long)x * bm->bpp;

        for (i = 0; i < n; i++) {
            p[i] = (unsigned char)(p[i] ^ *keep++);
        }
    }
}

/* A 16-bit value, most significant byte first or last. */
static int esf_word(unsigned int v, int little, unsigned char *out)
{
    unsigned char hi = (unsigned char)((v >> 8) & 0xFFu);
    unsigned char lo = (unsigned char)(v & 0xFFu);

    out[0] = little ? lo : hi;
    out[1] = little ? hi : lo;
    return 2;
}

/*
 * The layouts are CyberGraphX's (and Picasso96's, which follows them):
 *   RGB15   0rrrrrgg gggbbbbb      RGB16   rrrrrggg gggbbbbb
 *   BGR15   0bbbbbgg gggrrrrr      BGR16   bbbbbggg gggrrrrr
 * the PC variants are the same two bytes the other way round; then
 * RGB24, BGR24, ARGB32, BGRA32 and RGBA32 byte by byte. The alpha byte
 * is written as 0xFF; a screen does not look at it.
 */
int esf_raw_colour(unsigned long pixfmt, unsigned int r, unsigned int g,
                   unsigned int b, unsigned int pen, unsigned char *out)
{
    unsigned int r5 = (r & 0xFFu) >> 3, g5 = (g & 0xFFu) >> 3;
    unsigned int g6 = (g & 0xFFu) >> 2, b5 = (b & 0xFFu) >> 3;

    switch (pixfmt) {
    case 0:                                        /* LUT8     */
        out[0] = (unsigned char)(pen & 0xFFu);
        return 1;
    case 1:                                        /* RGB15    */
        return esf_word((r5 << 10) | (g5 << 5) | b5, 0, out);
    case 2:                                        /* BGR15    */
        return esf_word((b5 << 10) | (g5 << 5) | r5, 0, out);
    case 3:                                        /* RGB15PC  */
        return esf_word((r5 << 10) | (g5 << 5) | b5, 1, out);
    case 4:                                        /* BGR15PC  */
        return esf_word((b5 << 10) | (g5 << 5) | r5, 1, out);
    case 5:                                        /* RGB16    */
        return esf_word((r5 << 11) | (g6 << 5) | b5, 0, out);
    case 6:                                        /* BGR16    */
        return esf_word((b5 << 11) | (g6 << 5) | r5, 0, out);
    case 7:                                        /* RGB16PC  */
        return esf_word((r5 << 11) | (g6 << 5) | b5, 1, out);
    case 8:                                        /* BGR16PC  */
        return esf_word((b5 << 11) | (g6 << 5) | r5, 1, out);
    case 9:                                        /* RGB24    */
        out[0] = (unsigned char)r;
        out[1] = (unsigned char)g;
        out[2] = (unsigned char)b;
        return 3;
    case 10:                                       /* BGR24    */
        out[0] = (unsigned char)b;
        out[1] = (unsigned char)g;
        out[2] = (unsigned char)r;
        return 3;
    case 11:                                       /* ARGB32   */
        out[0] = 0xFF;
        out[1] = (unsigned char)r;
        out[2] = (unsigned char)g;
        out[3] = (unsigned char)b;
        return 4;
    case 12:                                       /* BGRA32   */
        out[0] = (unsigned char)b;
        out[1] = (unsigned char)g;
        out[2] = (unsigned char)r;
        out[3] = 0xFF;
        return 4;
    case 13:                                       /* RGBA32   */
        out[0] = (unsigned char)r;
        out[1] = (unsigned char)g;
        out[2] = (unsigned char)b;
        out[3] = 0xFF;
        return 4;
    default:
        return 0;
    }
}
