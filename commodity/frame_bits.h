/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * frame_bits.h - the preview frame's pixels, for a screen whose memory
 * can be touched directly: a planar bitmap of the native chipset, or
 * the linear memory of a graphics card.
 *
 * The frame is a masked XOR. Showing a strip keeps, for every bit of
 * it, what was there XOR the accent, and writes the accent; dropping it
 * XORs what was kept back. So the strip is exactly the accent while it
 * is up, whatever lies beneath, and dropping it gives back exactly what
 * was there. And because every change is an XOR, it composes with the
 * window outline Intuition draws by inversion during a drag, in any
 * order: an outline toggled over the strip while it is up is still
 * there, and still Intuition's to remove, once the strip is dropped.
 *
 * Pure C89, no system headers: the commodity hands in the plane
 * pointers or the base address and decides when it is safe to touch
 * them. Host-tested in frame_bits_test.c.
 */

#ifndef EDGESNAP_FRAME_BITS_H
#define EDGESNAP_FRAME_BITS_H

/* A planar bitmap as graphics.library lays it out: a pointer to the
 * first row of each plane and one stride for all of them, which holds
 * for interleaved bitmaps too. Pixel x is bit 7 - (x & 7) of byte
 * x >> 3, as on every Amiga. */
typedef struct ESFPlanes {
    unsigned char *plane[8];
    long stride;               /* bytes from a row of a plane to the next */
    int depth;                 /* 1 to 8 */
} ESFPlanes;

/* A chunky bitmap: rows of pixels of one to four bytes. */
typedef struct ESFChunky {
    unsigned char *base;
    long stride;               /* bytes from one row to the next */
    int bpp;                   /* 1 to 4 */
} ESFChunky;

/*
 * Bytes to keep for a strip of w x h pixels starting at column x, for
 * a planar bitmap of `depth` planes or a chunky one of `bpp` bytes per
 * pixel. 0 for an empty strip.
 */
unsigned long esf_planar_bytes(int x, int w, int h, int depth);
unsigned long esf_chunky_bytes(int w, int h, int bpp);

/*
 * Show a strip in pen `pen` (planar) or in the pixel `accent`, bpp
 * bytes as the screen stores them (chunky), keeping in `keep` what the
 * drop needs. The strip must lie inside the bitmap.
 */
void esf_planar_show(const ESFPlanes *bm, int x, int y, int w, int h,
                     unsigned int pen, unsigned char *keep);
void esf_chunky_show(const ESFChunky *bm, int x, int y, int w, int h,
                     const unsigned char *accent, unsigned char *keep);

/* Drop a strip shown with the same arguments. */
void esf_planar_drop(const ESFPlanes *bm, int x, int y, int w, int h,
                     const unsigned char *keep);
void esf_chunky_drop(const ESFChunky *bm, int x, int y, int w, int h,
                     const unsigned char *keep);

/*
 * A colour as a CyberGraphX pixel format stores it: `pen` for the
 * eight-bit lookup format, the 8-bit r, g, b otherwise. Writes the
 * bytes to `out` (room for four) and returns how many; 0 for a format
 * it does not know, which the caller takes for "draw it another way".
 */
int esf_raw_colour(unsigned long pixfmt, unsigned int r, unsigned int g,
                   unsigned int b, unsigned int pen, unsigned char *out);

#endif /* EDGESNAP_FRAME_BITS_H */
